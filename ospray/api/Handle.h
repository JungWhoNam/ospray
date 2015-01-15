// ======================================================================== //
// Copyright 2009-2014 Intel Corporation                                    //
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

#pragma once

/*! \todo move to api - it's also used in coi, and not mpi specific */

#include "ospray/common/OSPCommon.h"
#include "ospray/common/Managed.h"

namespace ospray {
  namespace api {

#define NULL_HANDLE (Handle(0))

    //! (local) handle to a (remote) managed object
    /*! abstraction for a remotely-held 'managed object'. the handle
      refers to both 'owner' (the machine that has it) as well as to a
      local ID (by which that owner can look it up). Note that other
      ranks may also have copies of that object. 

      note that the 'null handle' is '0', not -1. This allows an app
      to test the handled resturend from ospNewXXX calls for null just
      as if they were pointers (and thus, 'null' objects are
      consistent between local and mpi rendering)
    */
    struct Handle {
      static Handle alloc();
      void free();

      inline Handle(const int64 i) : i64(i) {};
      inline Handle(const Handle &other=NULL_HANDLE) : i64(other.i64) {};
      inline Handle &operator=(const Handle &other) { i64=other.i64; return *this; }

      union {
        struct { int32 ID; int32 owner; } i32;
        int64 i64;
      };

      /*! look up an object by handle, and return it. must be a defiend handle */
      ManagedObject *lookup() const;

      /*! Return the handle associated with the given object. */
      static Handle lookup(ManagedObject *object);

      /*! check whether the handle is defined *on this rank* */
      bool defined() const;

      /*! define the given handle to refer to given object */
      static void assign(const Handle &handle, const ManagedObject *object);

      /*! define the given handle to refer to given object */
      void assign(const ManagedObject *object) const;

      //! free the given object
      void freeObject() const;

      /*! returns the owner's rank */
      inline int32 owner() const { return i32.owner; }

      /*! returns the local ID to reference this object on the owner rank */
      inline int32 ID() const { return i32.ID; }

      /*! cast to int64 to allow fast operations with this type */
      inline operator int64() const { return i64; }

      static const Handle nullHandle;
    };

    inline bool operator==(const Handle &a, const Handle &b) { return a.i64==b.i64; }
    inline bool operator!=(const Handle &a, const Handle &b) { return a.i64!=b.i64; }

  } // ::ospray::api
} // ::ospray
