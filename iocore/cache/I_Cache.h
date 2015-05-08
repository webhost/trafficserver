/** @file

  A brief file description

  @section license License

  Licensed to the Apache Software Foundation (ASF) under one
  or more contributor license agreements.  See the NOTICE file
  distributed with this work for additional information
  regarding copyright ownership.  The ASF licenses this file
  to you under the Apache License, Version 2.0 (the
  "License"); you may not use this file except in compliance
  with the License.  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
 */

#ifndef _I_CACHE_H__
#define _I_CACHE_H__

#include "libts.h"
#include "I_EventSystem.h"
#include "I_AIO.h"
#include "I_CacheDefs.h"
#include "I_Store.h"

#define CACHE_MODULE_MAJOR_VERSION 1
#define CACHE_MODULE_MINOR_VERSION 0
#define CACHE_MODULE_VERSION makeModuleVersion(CACHE_MODULE_MAJOR_VERSION, CACHE_MODULE_MINOR_VERSION, PUBLIC_MODULE_HEADER)

#define CACHE_WRITE_OPT_OVERWRITE 0x0001
#define CACHE_WRITE_OPT_CLOSE_COMPLETE 0x0002
#define CACHE_WRITE_OPT_SYNC (CACHE_WRITE_OPT_CLOSE_COMPLETE | 0x0004)
#define CACHE_WRITE_OPT_OVERWRITE_SYNC (CACHE_WRITE_OPT_SYNC | CACHE_WRITE_OPT_OVERWRITE)

#define SCAN_KB_PER_SECOND 8192 // 1TB/8MB = 131072 = 36 HOURS to scan a TB

#define RAM_CACHE_ALGORITHM_CLFUS 0
#define RAM_CACHE_ALGORITHM_LRU 1

#define CACHE_COMPRESSION_NONE 0
#define CACHE_COMPRESSION_FASTLZ 1
#define CACHE_COMPRESSION_LIBZ 2
#define CACHE_COMPRESSION_LIBLZMA 3

struct CacheVConnection;
struct CacheVC;
struct CacheDisk;
#ifdef HTTP_CACHE
class CacheLookupHttpConfig;
class URL;
class HTTPHdr;
class HTTPInfo;
class HTTPRangeSpec;

typedef HTTPHdr CacheHTTPHdr;
typedef URL CacheURL;
typedef HTTPInfo CacheHTTPInfo;
#endif

struct CacheProcessor : public Processor {
  CacheProcessor()
    : min_stripe_version(CACHE_DB_MAJOR_VERSION, CACHE_DB_MINOR_VERSION),
      max_stripe_version(CACHE_DB_MAJOR_VERSION, CACHE_DB_MINOR_VERSION), cb_after_init(0)
  {
  }

  virtual int start(int n_cache_threads = 0, size_t stacksize = DEFAULT_STACKSIZE);
  virtual int start_internal(int flags = 0);
  void stop();

  int dir_check(bool fix);
  int db_check(bool fix);

  inkcoreapi Action *lookup(Continuation *cont, CacheKey *key, bool cluster_cache_local, bool local_only = false,
                            CacheFragType frag_type = CACHE_FRAG_TYPE_NONE, char *hostname = 0, int host_len = 0);
  inkcoreapi Action *open_read(Continuation *cont, CacheKey *key, bool cluster_cache_local,
                               CacheFragType frag_type = CACHE_FRAG_TYPE_NONE, char *hostname = 0, int host_len = 0);

  /** Open a cache reader from an already open writer.

      This is used for partial content on a cache miss to open a reader corresponding to the
      partial content writer.
  */
  inkcoreapi Action* open_read(Continuation* cont, CacheVConnection* writer, HTTPHdr* client_request_hdr);

  Action *open_read_buffer(Continuation *cont, MIOBuffer *buf, CacheKey *key,
                           CacheFragType frag_type = CACHE_FRAG_TYPE_NONE, char *hostname = 0, int host_len = 0);

  inkcoreapi Action *open_write(Continuation *cont,
                                CacheKey *key,
                                bool cluster_cache_local,
                                CacheFragType frag_type = CACHE_FRAG_TYPE_NONE,
                                int expected_size = CACHE_EXPECTED_SIZE,
                                int options = 0,
                                time_t pin_in_cache = (time_t) 0,
                                char *hostname = 0, int host_len = 0);
  Action *open_write_buffer(Continuation *cont, MIOBuffer *buf,
                            CacheKey *key,
                            CacheFragType frag_type = CACHE_FRAG_TYPE_NONE,
                            int options = 0,
                            time_t pin_in_cache = (time_t) 0,
                            char *hostname = 0, int host_len = 0);
  inkcoreapi Action *remove(Continuation *cont, CacheKey *key,
                            bool cluster_cache_local,
                            CacheFragType frag_type = CACHE_FRAG_TYPE_NONE,
                            bool rm_user_agents = true, bool rm_link = false,
                            char *hostname = 0, int host_len = 0);
  Action *scan(Continuation *cont, char *hostname = 0, int host_len = 0, int KB_per_second = SCAN_KB_PER_SECOND);
#ifdef HTTP_CACHE
  Action *lookup(Continuation *cont, URL *url, bool cluster_cache_local, bool local_only = false,
                 CacheFragType frag_type = CACHE_FRAG_TYPE_HTTP);
  inkcoreapi Action *open_read(Continuation *cont, URL *url, bool cluster_cache_local, CacheHTTPHdr *request,
                               CacheLookupHttpConfig *params, time_t pin_in_cache = (time_t)0,
                               CacheFragType frag_type = CACHE_FRAG_TYPE_HTTP);
  Action *open_write(Continuation *cont, int expected_size, URL *url, bool cluster_cache_local, CacheHTTPHdr *request,
                     CacheHTTPInfo *old_info, time_t pin_in_cache = (time_t)0, CacheFragType frag_type = CACHE_FRAG_TYPE_HTTP);
  Action *remove(Continuation *cont, URL *url, bool cluster_cache_local, CacheFragType frag_type = CACHE_FRAG_TYPE_HTTP);

  Action *open_read_internal(int, Continuation *, MIOBuffer *, CacheURL *, CacheHTTPHdr *, CacheLookupHttpConfig *, CacheKey *,
                             time_t, CacheFragType type, char *hostname, int host_len);
#endif
  Action *link(Continuation *cont, CacheKey *from, CacheKey *to, bool cluster_cache_local,
               CacheFragType frag_type = CACHE_FRAG_TYPE_HTTP, char *hostname = 0, int host_len = 0);

  Action *deref(Continuation *cont, CacheKey *key, bool cluster_cache_local, CacheFragType frag_type = CACHE_FRAG_TYPE_HTTP,
                char *hostname = 0, int host_len = 0);

  /** Mark physical disk/device/file as offline.
      All stripes for this device are disabled.

      @return @c true if there are any storage devices remaining online, @c false if not.

      @note This is what is called if a disk is disabled due to I/O errors.
  */
  bool mark_storage_offline(CacheDisk *d);

  /** Find the storage for a @a path.
      If @a len is 0 then @a path is presumed null terminated.
      @return @c NULL if the path does not match any defined storage.
   */
  CacheDisk *find_by_path(char const *path, int len = 0);

  /** Check if there are any online storage devices.
      If this returns @c false then the cache should be disabled as there is no storage available.
  */
  bool has_online_storage() const;

  /** Get the target fragment size. */
  int64_t get_fixed_fragment_size() const;

  static int IsCacheEnabled();

  static bool IsCacheReady(CacheFragType type);

  /// Type for callback function.
  typedef void (*CALLBACK_FUNC)();
  /** Lifecycle callback.

      The function @a cb is called after cache initialization has
      finished and the cache is ready or has failed.

      @internal If we need more lifecycle callbacks, this should be
      generalized ala the standard hooks style, with a type enum used
      to specific the callback type and passed to the callback
      function.
  */
  void set_after_init_callback(CALLBACK_FUNC cb);

  // private members
  void diskInitialized();

  void cacheInitialized();

  static volatile uint32_t cache_ready;
  static volatile int initialized;
  static volatile int start_done;
  static int clear;
  static int fix;
  static int start_internal_flags;
  static int auto_clear_flag;

  VersionNumber min_stripe_version;
  VersionNumber max_stripe_version;

  CALLBACK_FUNC cb_after_init;
};

inline void
CacheProcessor::set_after_init_callback(CALLBACK_FUNC cb)
{
  cb_after_init = cb;
}

struct CacheVConnection : public VConnection {
  VIO *do_io_read(Continuation *c, int64_t nbytes, MIOBuffer *buf) = 0;
  virtual VIO *do_io_pread(Continuation *c, int64_t nbytes, MIOBuffer *buf, int64_t offset) = 0;
  VIO *do_io_write(Continuation *c, int64_t nbytes, IOBufferReader *buf, bool owner = false) = 0;
  void do_io_close(int lerrno = -1) = 0;
  void reenable(VIO *avio) = 0;
  void reenable_re(VIO *avio) = 0;
  void
  do_io_shutdown(ShutdownHowTo_t howto)
  {
    (void)howto;
    ink_assert(!"CacheVConnection::do_io_shutdown unsupported");
  }

  virtual int get_header(void **ptr, int *len) = 0;
  virtual int set_header(void *ptr, int len) = 0;
  virtual int get_single_data(void **ptr, int *len) = 0;

#ifdef HTTP_CACHE
  virtual void set_http_info(CacheHTTPInfo *info) = 0;
  virtual void get_http_info(CacheHTTPInfo **info) = 0;

  /** Get the boundary string for a multi-part range response.
      The length of the string is returned in @a len.

      @return A point to the string.
   */
  virtual char const* get_http_range_boundary_string(int* len) const = 0;

  /** Get the effective content size.

      This is the amount of actual data based on any range or framing.  Effectively this is the
      value to be passed to the @c VIO while the content length is used in the HTTP header.
  */
  virtual int64_t get_effective_content_size() = 0;

  /** Set the origin reported content size.

      This is the content length reported by the origin server and should be considered a hint, not
      definitive. The object size, as stored in the cache, is the actual amount of data received and
      cached.

      @note This is the total content length as reported in the HTTP header, not the partial (range based) response size.
      Also this is the length of the HTTP content, which may differ from the size of the data stream.
  */
  virtual void set_full_content_length(int64_t) = 0;

  /** Set the output ranges for the content.
   */
  virtual void set_content_range(HTTPRangeSpec const& range) = 0;

  /// Get the unchanged ranges for the request range @a req.
  /// If @a req is empty it is treated as a full request (non-partial).
  /// @return @c true if the @a result is not empty.
  /// @internal Currently this just returns the single range that is convex hull of the uncached request.
  /// Someday we may want to do the exact range spec but we use the type for now because it's easier.
  virtual bool get_uncached(HTTPRangeSpec const& req, HTTPRangeSpec& result) { (void)req; (void)result; return false; }

  /** Set the range for the input (response content).
      The incoming bytes will be written to this section of the object.
      @note This range @b must be absolute.
      @note The range is inclusive.
      @return The # of bytes in the range.
  */
  virtual int64_t set_inbound_range(int64_t min, int64_t max) { return 1 + (max - min); }

#endif

  virtual bool is_ram_cache_hit() const = 0;
  virtual bool set_disk_io_priority(int priority) = 0;
  virtual int get_disk_io_priority() = 0;
  virtual bool set_pin_in_cache(time_t t) = 0;
  virtual time_t get_pin_in_cache() = 0;
  virtual int64_t get_object_size() = 0;

  /** Test if the VC can support pread.
      @return @c true if @c do_io_pread will work, @c false if not.
  */
  virtual bool is_pread_capable() = 0;

  CacheVConnection();
};

void ink_cache_init(ModuleVersion version);
extern inkcoreapi CacheProcessor cacheProcessor;
extern Continuation *cacheRegexDeleteCont;

#endif /* _I_CACHE_H__ */
