// ======================================================================== //
// Copyright 2009-2016 Intel Corporation                                    //
//                                                                          //
// Licensed under the Apache License, Version 2.0 (the "License");          //
// you may not use this file except in compliance with the License.         //
// You may obtain a copy of the License at                                  //
//                                                                          //
//     http://www.apache.org/licenses/LICENSE-2.0                           //
//                                                                          //
// Unless required by applicable law or agreed to in writing, software      //
// distributed under the License is distributed on an "AS IS" BASIS,        //
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. //
// See the License for the specific language governing permissions and      //
// limitations under the License.                                           //
// ======================================================================== //

#include "state.h"
#include "../../common/lexers/streamfilters.h"

namespace embree
{
  State::State (bool singledevice) 
    : thread_error(createTls()), cpu_features(getCPUFeatures())
  {
    tri_accel = "default";
    tri_builder = "default";
    tri_traverser = "default";
    tri_builder_replication_factor = 2.0f;
    
    tri_accel_mb = "default";
    tri_builder_mb = "default";
    tri_traverser_mb = "default";
    
    hair_accel = "default";
    hair_builder = "default";
    hair_traverser = "default";
    hair_builder_replication_factor = 3.0f;

    memory_preallocation_factor     = 1.0f; 

    tessellation_cache_size = 128*1024*1024;

    /* large default cache size only for old mode single device mode */
#if defined(__X86_64__)
      if (singledevice) tessellation_cache_size = 1024*1024*1024;
#else
      if (singledevice) tessellation_cache_size = 128*1024*1024;
#endif

    subdiv_accel = "default";

    instancing_open_min = 0;
    instancing_block_size = 0;
    instancing_open_factor = 8.0f; 
    instancing_open_max_depth = 32;
    instancing_open_max = 50000000;

    float_exceptions = false;
    scene_flags = -1;
    verbose = 0;
    benchmark = 0;
    regression_testing = 0;

    numThreads = 0;
#if TASKING_TBB_INTERNAL || defined(__MIC__)
    set_affinity = true;
#else
    set_affinity = false;
#endif

    error_function = nullptr;
    memory_monitor_function = nullptr;
  }

  State::~State() 
  {
    Lock<MutexSys> lock(errors_mutex);
    for (size_t i=0; i<thread_errors.size(); i++)
      delete thread_errors[i];
    destroyTls(thread_error);
    thread_errors.clear();
  }

  bool State::hasISA(const int isa) {
    return (cpu_features & isa) == isa;
  }

  void State::verify()
  {
    /* CPU has to support at least SSE2 */
#if !defined (__MIC__)
    if (!hasISA(SSE2)) 
      throw_RTCError(RTC_UNSUPPORTED_CPU,"CPU does not support SSE2");
#endif

#if defined(__MIC__)
    if (!(numThreads == 1 || (numThreads % 4) == 0))
      throw_RTCError(RTC_INVALID_OPERATION,"Xeon Phi supports only number of threads % 4 == 0, or threads == 1");
#endif

    /* verify that calculations stay in range */
    assert(rcp(min_rcp_input)*FLT_LARGE+FLT_LARGE < 0.01f*FLT_MAX);

    /* here we verify that CPP files compiled for a specific ISA only
     * call that same or lower ISA version of non-inlined class member
     * functions */
#if !defined (__MIC__) && defined(DEBUG)
    assert(isa::getISA() == ISA);
#if defined(__TARGET_SSE41__)
    assert(sse41::getISA() <= SSE41);
#endif
#if defined(__TARGET_SSE42__)
    assert(sse42::getISA() <= SSE42);
#endif
#if defined(__TARGET_AVX__)
    assert(avx::getISA() <= AVX);
#endif
#if defined(__TARGET_AVX2__)
    assert(avx2::getISA() <= AVX2);
#endif
#if defined (__TARGET_AVX512__)
    assert(avx512::getISA() <= AVX512KNL);
#endif
#endif
  }

  const char* symbols[3] = { "=", ",", "|" };

   bool State::parseFile(const FileName& fileName)
  {
    FILE* f = fopen(fileName.c_str(),"r");
    if (f == nullptr) return false;
    Ref<Stream<int> > file = new FileStream(f,fileName);

    std::vector<std::string> syms;
	  for (size_t i=0; i<sizeof(symbols)/sizeof(void*); i++) 
      syms.push_back(symbols[i]);

    Ref<TokenStream> cin = new TokenStream(new LineCommentFilter(file,"#"),
                                           TokenStream::alpha+TokenStream::ALPHA+TokenStream::numbers+"_.",
                                           TokenStream::separators,syms);
    parse(cin);
    return true;
  }

  void State::parseString(const char* cfg)
  {
    if (cfg == nullptr) return;

    std::vector<std::string> syms;
    for (size_t i=0; i<sizeof(symbols)/sizeof(void*); i++) 
      syms.push_back(symbols[i]);
    
    Ref<TokenStream> cin = new TokenStream(new StrStream(cfg),
                                           TokenStream::alpha+TokenStream::ALPHA+TokenStream::numbers+"_.",
                                           TokenStream::separators,syms);
    parse(cin);
  }
  
  int string_to_cpufeatures(const std::string& isa)
  {
    if      (isa == "sse" ) return SSE;
    else if (isa == "sse2") return SSE2;
    else if (isa == "sse3") return SSE3;
    else if (isa == "ssse3") return SSSE3;
    else if (isa == "sse41") return SSE41;
    else if (isa == "sse4.1") return SSE41;
    else if (isa == "sse42") return SSE42;
    else if (isa == "sse4.2") return SSE42;
    else if (isa == "avx") return AVX;
    else if (isa == "avxi") return AVXI;
    else if (isa == "avx2") return AVX2;
    else return SSE2;
  }

  void State::parse(Ref<TokenStream> cin)
  {
    /* parse until end of stream */
    while (cin->peek() != Token::Eof())
    {
      const Token tok = cin->get();

      if (tok == Token::Id("threads") && cin->trySymbol("=")) 
        numThreads = cin->get().Int();
      
      else if (tok == Token::Id("set_affinity")&& cin->trySymbol("=")) 
        set_affinity = cin->get().Int();
      
      else if (tok == Token::Id("isa") && cin->trySymbol("=")) {
        std::string isa = strlwr(cin->get().Identifier());
        cpu_features = string_to_cpufeatures(isa);
      }

      else if (tok == Token::Id("max_isa") && cin->trySymbol("=")) {
        std::string isa = strlwr(cin->get().Identifier());
        cpu_features &= string_to_cpufeatures(isa);
      }

      else if (tok == Token::Id("float_exceptions") && cin->trySymbol("=")) 
        float_exceptions = cin->get().Int();

      else if ((tok == Token::Id("tri_accel") || tok == Token::Id("accel")) && cin->trySymbol("="))
        tri_accel = cin->get().Identifier();
      else if ((tok == Token::Id("tri_builder") || tok == Token::Id("builder")) && cin->trySymbol("="))
        tri_builder = cin->get().Identifier();
      else if ((tok == Token::Id("tri_traverser") || tok == Token::Id("traverser")) && cin->trySymbol("="))
        tri_traverser = cin->get().Identifier();
      else if (tok == Token::Id("tri_builder_replication_factor") && cin->trySymbol("="))
        tri_builder_replication_factor = cin->get().Int();
      
      else if ((tok == Token::Id("tri_accel_mb") || tok == Token::Id("accel_mb")) && cin->trySymbol("="))
        tri_accel_mb = cin->get().Identifier();
      else if ((tok == Token::Id("tri_builder_mb") || tok == Token::Id("builder_mb")) && cin->trySymbol("="))
        tri_builder_mb = cin->get().Identifier();
      else if ((tok == Token::Id("tri_traverser_mb") || tok == Token::Id("traverser_mb")) && cin->trySymbol("="))
        tri_traverser_mb = cin->get().Identifier();
      
      else if (tok == Token::Id("hair_accel") && cin->trySymbol("="))
        hair_accel = cin->get().Identifier();
      else if (tok == Token::Id("hair_builder") && cin->trySymbol("="))
        hair_builder = cin->get().Identifier();
      else if (tok == Token::Id("hair_traverser") && cin->trySymbol("="))
        hair_traverser = cin->get().Identifier();
      else if (tok == Token::Id("hair_builder_replication_factor") && cin->trySymbol("="))
        hair_builder_replication_factor = cin->get().Int();

      else if (tok == Token::Id("instancing_open_min") && cin->trySymbol("="))
        instancing_open_min = cin->get().Int();
      else if (tok == Token::Id("instancing_block_size") && cin->trySymbol("=")) {
        instancing_block_size = cin->get().Int();
        instancing_open_factor = 0.0f;
      }
      else if (tok == Token::Id("instancing_open_max_depth") && cin->trySymbol("="))
        instancing_open_max_depth = cin->get().Int();
      else if (tok == Token::Id("instancing_open_factor") && cin->trySymbol("=")) {
        instancing_block_size = 0;
        instancing_open_factor = cin->get().Float();
      }
      else if (tok == Token::Id("instancing_open_max") && cin->trySymbol("="))
        instancing_open_max = cin->get().Int();

      else if (tok == Token::Id("subdiv_accel") && cin->trySymbol("="))
        subdiv_accel = cin->get().Identifier();
      
      else if (tok == Token::Id("verbose") && cin->trySymbol("="))
        verbose = cin->get().Int();
      else if (tok == Token::Id("benchmark") && cin->trySymbol("="))
        benchmark = cin->get().Int();
      
      else if (tok == Token::Id("flags")) {
        scene_flags = 0;
        if (cin->trySymbol("=")) {
          do {
            Token flag = cin->get();
            if      (flag == Token::Id("static") ) scene_flags |= RTC_SCENE_STATIC;
            else if (flag == Token::Id("dynamic")) scene_flags |= RTC_SCENE_DYNAMIC;
            else if (flag == Token::Id("compact")) scene_flags |= RTC_SCENE_COMPACT;
            else if (flag == Token::Id("coherent")) scene_flags |= RTC_SCENE_COHERENT;
            else if (flag == Token::Id("incoherent")) scene_flags |= RTC_SCENE_INCOHERENT;
            else if (flag == Token::Id("high_quality")) scene_flags |= RTC_SCENE_HIGH_QUALITY;
            else if (flag == Token::Id("robust")) scene_flags |= RTC_SCENE_ROBUST;
          } while (cin->trySymbol("|"));
        }
      }
      else if (tok == Token::Id("memory_preallocation_factor") && cin->trySymbol("=")) 
        memory_preallocation_factor = cin->get().Float();
      
      else if (tok == Token::Id("regression") && cin->trySymbol("=")) 
        regression_testing = cin->get().Int();
      
      else if (tok == Token::Id("tessellation_cache_size") && cin->trySymbol("="))
        tessellation_cache_size = cin->get().Float() * 1024 * 1024;

      cin->trySymbol(","); // optional , separator
    }
  }

  RTCError* State::error() 
  {
    RTCError* stored_error = (RTCError*) getTls(thread_error);
    if (stored_error == nullptr) {
      Lock<MutexSys> lock(errors_mutex);
      stored_error = new RTCError(RTC_NO_ERROR);
      thread_errors.push_back(stored_error);
      setTls(thread_error,stored_error);
    }
    return stored_error;
  }

  bool State::verbosity(int N) {
    return N <= verbose;
  }

  void State::print()
  {
    std::cout << "general:" << std::endl;
    std::cout << "  build threads = " << numThreads << std::endl;
    std::cout << "  verbosity     = " << verbose << std::endl;
    
    std::cout << "triangles:" << std::endl;
    std::cout << "  accel         = " << tri_accel << std::endl;
    std::cout << "  builder       = " << tri_builder << std::endl;
    std::cout << "  traverser     = " << tri_traverser << std::endl;
    std::cout << "  replications  = " << tri_builder_replication_factor << std::endl;
    
    std::cout << "motion blur triangles:" << std::endl;
    std::cout << "  accel         = " << tri_accel_mb << std::endl;
    std::cout << "  builder       = " << tri_builder_mb << std::endl;
    std::cout << "  traverser     = " << tri_traverser_mb << std::endl;
    
    std::cout << "hair:" << std::endl;
    std::cout << "  accel         = " << hair_accel << std::endl;
    std::cout << "  builder       = " << hair_builder << std::endl;
    std::cout << "  traverser     = " << hair_traverser << std::endl;
    std::cout << "  replications  = " << hair_builder_replication_factor << std::endl;
    
    std::cout << "subdivision surfaces:" << std::endl;
    std::cout << "  accel         = " << subdiv_accel << std::endl;
    
#if defined(__MIC__)
    std::cout << "memory allocation:" << std::endl;
    std::cout << "  preallocation_factor  = " << memory_preallocation_factor << std::endl;
#endif
  }
}
