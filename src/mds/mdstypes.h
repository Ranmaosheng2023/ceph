// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*- 
// vim: ts=8 sw=2 smarttab
#ifndef __MDSTYPES_H
#define __MDSTYPES_H


#include <math.h>
#include <ostream>
#include <set>
#include <map>
using namespace std;

#include "config.h"
#include "common/DecayCounter.h"
#include "include/Context.h"

#include <cassert>

#include "include/frag.h"
#include "include/xlist.h"
#include "include/nstring.h"

#define MDS_REF_SET      // define me for improved debug output, sanity checking
//#define MDS_AUTHPIN_SET  // define me for debugging auth pin leaks
//#define MDS_VERIFY_FRAGSTAT    // do do (slow) sanity checking on frags

#define MDS_PORT_CACHE   0x200
#define MDS_PORT_LOCKER  0x300
#define MDS_PORT_MIGRATOR 0x400


#define MAX_MDS                   0x100

#define MDS_INO_ROOT              1
#define MDS_INO_PGTABLE           2
#define MDS_INO_ANCHORTABLE       3
#define MDS_INO_SNAPTABLE         4

#define MDS_INO_LOG_OFFSET        (1*MAX_MDS)
#define MDS_INO_IDS_OFFSET        (2*MAX_MDS)
#define MDS_INO_CLIENTMAP_OFFSET  (3*MAX_MDS)
#define MDS_INO_SESSIONMAP_OFFSET (4*MAX_MDS)
#define MDS_INO_STRAY_OFFSET      (5*MAX_MDS)
#define MDS_INO_BASE              (6*MAX_MDS)

#define MDS_INO_STRAY(x) (MDS_INO_STRAY_OFFSET+((unsigned)x))
#define MDS_INO_IS_STRAY(i) ((i) >= MDS_INO_STRAY_OFFSET && (i) < MDS_INO_STRAY_OFFSET+MAX_MDS)

#define MDS_TRAVERSE_FORWARD       1
#define MDS_TRAVERSE_DISCOVER      2    // skips permissions checks etc.
#define MDS_TRAVERSE_DISCOVERXLOCK 3    // succeeds on (foreign?) null, xlocked dentries.
#define MDS_TRAVERSE_FAIL          4



typedef __u32 capseq_t;

inline string cap_string(int cap)
{
  string s;
  s = "[";
  if (cap & CEPH_CAP_PIN) s += " pin";
  if (cap & CEPH_CAP_RDCACHE) s += " rdcache";
  if (cap & CEPH_CAP_RD) s += " rd";
  if (cap & CEPH_CAP_WR) s += " wr";
  if (cap & CEPH_CAP_WRBUFFER) s += " wrbuffer";
  if (cap & CEPH_CAP_WRBUFFER) s += " wrextend";
  if (cap & CEPH_CAP_LAZYIO) s += " lazyio";
  if (cap & CEPH_CAP_EXCL) s += " excl";
  s += " ]";
  return s;
}


struct frag_info_t {
  version_t version;

  // this frag
  utime_t mtime;
  __s64 nfiles;        // files
  __s64 nsubdirs;      // subdirs
  __s64 size() const { return nfiles + nsubdirs; }

  // this frag + children
  utime_t rctime;
  __s64 rbytes;
  __s64 rfiles;
  __s64 rsubdirs;
  __s64 rsize() const { return rfiles + rsubdirs; }
  __s64 ranchors;  // for dirstat, includes inode's anchored flag.

  void zero() {
    memset(this, 0, sizeof(*this));
  }
  void take_diff(const frag_info_t &cur, frag_info_t &acc, bool& touched_mtime) {
    if (cur.mtime > mtime) {
      rctime = mtime = cur.mtime;
      touched_mtime = true;
    }
    nfiles += cur.nfiles - acc.nfiles;
    nsubdirs += cur.nsubdirs - acc.nsubdirs;

    if (cur.rctime > rctime)
      rctime = cur.rctime;
    rbytes += cur.rbytes - acc.rbytes;
    rfiles += cur.rfiles - acc.rfiles;
    rsubdirs += cur.rsubdirs - acc.rsubdirs;
    ranchors += cur.ranchors - acc.ranchors;
    acc = cur;
    acc.version = version;
  }

  void encode(bufferlist &bl) const {
    ::encode(version, bl);
    ::encode(mtime, bl);
    ::encode(nfiles, bl);
    ::encode(nsubdirs, bl);
    ::encode(rbytes, bl);
    ::encode(rfiles, bl);
    ::encode(rsubdirs, bl);
    ::encode(ranchors, bl);
    ::encode(rctime, bl);
  }
  void decode(bufferlist::iterator &bl) {
    ::decode(version, bl);
    ::decode(mtime, bl);
    ::decode(nfiles, bl);
    ::decode(nsubdirs, bl);
    ::decode(rbytes, bl);
    ::decode(rfiles, bl);
    ::decode(rsubdirs, bl);
    ::decode(ranchors, bl);
    ::decode(rctime, bl);
 }
};
WRITE_CLASS_ENCODER(frag_info_t)

inline bool operator==(const frag_info_t &l, const frag_info_t &r) {
  return memcmp(&l, &r, sizeof(l)) == 0;
}

inline ostream& operator<<(ostream &out, const frag_info_t &f) {
  return out << "f(v" << f.version
	     << " m" << f.mtime
	     << " " << f.size() << "=" << f.nfiles << "+" << f.nsubdirs
	     << " rc" << f.rctime
	     << " b" << f.rbytes
	     << " a" << f.ranchors
	     << " " << f.rsize() << "=" << f.rfiles << "+" << f.rsubdirs
	     << ")";    
}

struct vinodeno_t {
  inodeno_t ino;
  snapid_t snapid;
  vinodeno_t() {}
  vinodeno_t(inodeno_t i, snapid_t s) : ino(i), snapid(s) {}
};

inline bool operator==(const vinodeno_t &l, const vinodeno_t &r) {
  return l.ino == r.ino && l.snapid == r.snapid;
}

namespace __gnu_cxx {
  template<> struct hash<vinodeno_t> {
    size_t operator()(const vinodeno_t &vino) const { 
      hash<inodeno_t> H;
      hash<uint64_t> I;
      return H(vino.ino) ^ I(vino.snapid);
    }
  };
}




inline ostream& operator<<(ostream &out, const vinodeno_t &vino) {
  out << vino.ino;
  if (vino.snapid == CEPH_NOSNAP)
    out << ".head";
  else if (vino.snapid)
    out << '.' << vino.snapid;
  return out;
}


struct inode_t {
  // base (immutable)
  inodeno_t ino;
  ceph_file_layout layout;  // ?immutable?
  uint32_t   rdev;    // if special file

  // affected by any inode change...
  utime_t    ctime;   // inode change time

  // perm (namespace permissions)
  uint32_t   mode;
  uid_t      uid;
  gid_t      gid;

  // nlink
  int32_t    nlink;  
  bool       anchored;          // auth only?

  // file (data access)
  uint64_t   size;        // on directory, # dentries
  uint64_t   max_size;    // client(s) are auth to write this much...
  utime_t    mtime;   // file data modify time.
  utime_t    atime;   // file data access time.
  uint64_t   time_warp_seq;  // count of (potential) mtime/atime timewarps (i.e., utimes())

  // dirfrag, recursive accounting
  frag_info_t dirstat;             
  frag_info_t accounted_dirstat;   // what dirfrag has seen
 
  // special stuff
  version_t version;           // auth only
  version_t file_data_version; // auth only

  // file type
  bool is_symlink() const { return (mode & S_IFMT) == S_IFLNK; }
  bool is_dir()     const { return (mode & S_IFMT) == S_IFDIR; }
  bool is_file()    const { return (mode & S_IFMT) == S_IFREG; }

  void encode(bufferlist &bl) const {
    ::encode(ino, bl);
    ::encode(layout, bl);
    ::encode(rdev, bl);
    ::encode(ctime, bl);

    ::encode(mode, bl);
    ::encode(uid, bl);
    ::encode(gid, bl);

    ::encode(nlink, bl);
    ::encode(anchored, bl);

    ::encode(size, bl);
    ::encode(max_size, bl);
    ::encode(mtime, bl);
    ::encode(atime, bl);
    ::encode(time_warp_seq, bl);

    ::encode(dirstat, bl);
    ::encode(accounted_dirstat, bl);

    ::encode(version, bl);
    ::encode(file_data_version, bl);
  }
  void decode(bufferlist::iterator &p) {
    ::decode(ino, p);
    ::decode(layout, p);
    ::decode(rdev, p);
    ::decode(ctime, p);

    ::decode(mode, p);
    ::decode(uid, p);
    ::decode(gid, p);

    ::decode(nlink, p);
    ::decode(anchored, p);

    ::decode(size, p);
    ::decode(max_size, p);
    ::decode(mtime, p);
    ::decode(atime, p);
    ::decode(time_warp_seq, p);
    
    ::decode(dirstat, p);
    ::decode(accounted_dirstat, p);

    ::decode(version, p);
    ::decode(file_data_version, p);
  }
};
WRITE_CLASS_ENCODER(inode_t)


struct old_inode_t {
  snapid_t first;
  inode_t inode;
  map<string,bufferptr> xattrs;

  void encode(bufferlist& bl) const {
    ::encode(first, bl);
    ::encode(inode, bl);
    ::encode(xattrs, bl);
  }
  void decode(bufferlist::iterator& bl) {
    ::decode(first, bl);
    ::decode(inode, bl);
    ::decode(xattrs, bl);
  }
};
WRITE_CLASS_ENCODER(old_inode_t)


/*
 * like an inode, but for a dir frag 
 */
struct fnode_t {
  version_t version;
  frag_info_t fragstat, accounted_fragstat;

  void encode(bufferlist &bl) const {
    ::encode(version, bl);
    ::encode(fragstat, bl);
    ::encode(accounted_fragstat, bl);
  }
  void decode(bufferlist::iterator &bl) {
    ::decode(version, bl);
    ::decode(fragstat, bl);
    ::decode(accounted_fragstat, bl);
  }
};
WRITE_CLASS_ENCODER(fnode_t)




// =======
// dentries

typedef pair<snapid_t, const char *> dentry_key_t;

struct ltdentrykey
{
  bool operator()(const dentry_key_t& k1,
		  const dentry_key_t& k2) const
  {
    return 
      k1.first < k2.first ||
      (k1.first == k2.first && strcmp(k1.second, k2.second) < 0);
  }
};




// =========
// requests

struct metareqid_t {
  entity_name_t name;
  __u64 tid;
  metareqid_t() : tid(0) {}
  metareqid_t(entity_name_t n, tid_t t) : name(n), tid(t) {}
};

static inline void encode(const metareqid_t &r, bufferlist &bl)
{
  ::encode(r.name, bl);
  ::encode(r.tid, bl);
}
static inline void decode( metareqid_t &r, bufferlist::iterator &p)
{
  ::decode(r.name, p);
  ::decode(r.tid, p);
}

inline ostream& operator<<(ostream& out, const metareqid_t& r) {
  return out << r.name << ":" << r.tid;
}

inline bool operator==(const metareqid_t& l, const metareqid_t& r) {
  return (l.name == r.name) && (l.tid == r.tid);
}
inline bool operator!=(const metareqid_t& l, const metareqid_t& r) {
  return (l.name != r.name) || (l.tid != r.tid);
}
inline bool operator<(const metareqid_t& l, const metareqid_t& r) {
  return (l.name < r.name) || 
    (l.name == r.name && l.tid < r.tid);
}
inline bool operator<=(const metareqid_t& l, const metareqid_t& r) {
  return (l.name < r.name) ||
    (l.name == r.name && l.tid <= r.tid);
}
inline bool operator>(const metareqid_t& l, const metareqid_t& r) { return !(l <= r); }
inline bool operator>=(const metareqid_t& l, const metareqid_t& r) { return !(l < r); }

namespace __gnu_cxx {
  template<> struct hash<metareqid_t> {
    size_t operator()(const metareqid_t &r) const { 
      hash<uint64_t> H;
      return H(r.name.num()) ^ H(r.name.type()) ^ H(r.tid);
    }
  };
}


// inode caps info for client reconnect
struct inode_caps_reconnect_t {
  int32_t wanted;
  int32_t issued;
  uint64_t size;
  utime_t mtime, atime;

  inode_caps_reconnect_t() {}
  inode_caps_reconnect_t(int w, int i) : 
    wanted(w), issued(i), size(0) {}
  inode_caps_reconnect_t(int w, int i, uint64_t sz, utime_t mt, utime_t at) : 
    wanted(w), issued(i), size(sz), mtime(mt), atime(at) {}
};

static inline void encode(const inode_caps_reconnect_t &ic, bufferlist &bl)
{
  ::encode(ic.wanted, bl);
  ::encode(ic.issued, bl);
  ::encode(ic.size, bl);
  ::encode(ic.mtime, bl);
  ::encode(ic.atime, bl);
}
static inline void decode(inode_caps_reconnect_t &ic, bufferlist::iterator &p)
{
  ::decode(ic.wanted, p);
  ::decode(ic.issued, p);
  ::decode(ic.size, p);
  ::decode(ic.mtime, p);
  ::decode(ic.atime, p);
}


// ================================================================
// dir frag

struct dirfrag_t {
  inodeno_t ino;
  frag_t    frag;
  uint32_t  _pad;

  dirfrag_t() : ino(0), _pad(0) { }
  dirfrag_t(inodeno_t i, frag_t f) : ino(i), frag(f), _pad(0) { }
};

inline void encode(const dirfrag_t &f, bufferlist& bl) { 
  encode(f.ino, bl);
  encode(f.frag, bl);
}
inline void decode(dirfrag_t &f, bufferlist::iterator& p) { 
  decode(f.ino, p);
  decode(f.frag, p);
}


inline ostream& operator<<(ostream& out, const dirfrag_t df) {
  out << df.ino;
  if (!df.frag.is_root()) out << "." << df.frag;
  return out;
}
inline bool operator<(dirfrag_t l, dirfrag_t r) {
  if (l.ino < r.ino) return true;
  if (l.ino == r.ino && l.frag < r.frag) return true;
  return false;
}
inline bool operator==(dirfrag_t l, dirfrag_t r) {
  return l.ino == r.ino && l.frag == r.frag;
}

namespace __gnu_cxx {
  template<> struct hash<dirfrag_t> {
    size_t operator()(const dirfrag_t &df) const { 
      static rjhash<uint64_t> H;
      static rjhash<uint32_t> I;
      return H(df.ino) ^ I(df.frag);
    }
  };
}



// ================================================================

#define META_POP_IRD     0
#define META_POP_IWR     1
#define META_POP_READDIR 2
#define META_POP_FETCH   3
#define META_POP_STORE   4
#define META_NPOP        5

class inode_load_vec_t {
  static const int NUM = 2;
  DecayCounter vec[NUM];
public:
  DecayCounter &get(int t) { 
    assert(t < NUM);
    return vec[t]; 
  }
  void zero(utime_t now) {
    for (int i=0; i<NUM; i++) 
      vec[i].reset(now);
  }
  void encode(bufferlist &bl) const {
    for (int i=0; i<NUM; i++)
      ::encode(vec[i], bl);
  }
  void decode(bufferlist::iterator &p) {
    for (int i=0; i<NUM; i++)
      ::decode(vec[i], p);
  }
};
WRITE_CLASS_ENCODER(inode_load_vec_t)

class dirfrag_load_vec_t {
public:
  static const int NUM = 5;
  DecayCounter vec[NUM];

  void encode(bufferlist &bl) const {
    for (int i=0; i<NUM; i++)
      ::encode(vec[i], bl);
  }
  void decode(bufferlist::iterator &p) {
    for (int i=0; i<NUM; i++)
      ::decode(vec[i], p);
  }

  DecayCounter &get(int t) { 
    assert(t < NUM);
    return vec[t]; 
  }
  void adjust(utime_t now, double d) {
    for (int i=0; i<NUM; i++) 
      vec[i].adjust(now, d);
  }
  void zero(utime_t now) {
    for (int i=0; i<NUM; i++) 
      vec[i].reset(now);
  }
  double meta_load(utime_t now) {
    return 
      1*vec[META_POP_IRD].get(now) + 
      2*vec[META_POP_IWR].get(now) +
      1*vec[META_POP_READDIR].get(now) +
      2*vec[META_POP_FETCH].get(now) +
      4*vec[META_POP_STORE].get(now);
  }
  double meta_load() {
    return 
      1*vec[META_POP_IRD].get_last() + 
      2*vec[META_POP_IWR].get_last() +
      1*vec[META_POP_READDIR].get_last() +
      2*vec[META_POP_FETCH].get_last() +
      4*vec[META_POP_STORE].get_last();
  }
};

WRITE_CLASS_ENCODER(dirfrag_load_vec_t)

inline dirfrag_load_vec_t& operator+=(dirfrag_load_vec_t& l, dirfrag_load_vec_t& r)
{
  utime_t now = g_clock.now();
  for (int i=0; i<dirfrag_load_vec_t::NUM; i++)
    l.vec[i].adjust(r.vec[i].get(now));
  return l;
}

inline dirfrag_load_vec_t& operator-=(dirfrag_load_vec_t& l, dirfrag_load_vec_t& r)
{
  utime_t now = g_clock.now();
  for (int i=0; i<dirfrag_load_vec_t::NUM; i++)
    l.vec[i].adjust(-r.vec[i].get(now));
  return l;
}

inline dirfrag_load_vec_t& operator*=(dirfrag_load_vec_t& l, double f)
{
  for (int i=0; i<dirfrag_load_vec_t::NUM; i++)
    l.vec[i].scale(f);
  return l;
}


inline ostream& operator<<(ostream& out, dirfrag_load_vec_t& dl)
{
  utime_t now = g_clock.now();
  return out << "[" << dl.vec[0].get(now) << "," << dl.vec[1].get(now) 
	     << " " << dl.meta_load(now)
	     << "]";
}






/* mds_load_t
 * mds load
 */

struct mds_load_t {
  dirfrag_load_vec_t auth;
  dirfrag_load_vec_t all;

  double req_rate;
  double cache_hit_rate;
  double queue_len;

  double cpu_load_avg;

  mds_load_t() : 
    req_rate(0), cache_hit_rate(0), queue_len(0), cpu_load_avg(0) { 
  }
  
  double mds_load();  // defiend in MDBalancer.cc

  void encode(bufferlist &bl) const {
    ::encode(auth, bl);
    ::encode(all, bl);
    ::encode(req_rate, bl);
    ::encode(cache_hit_rate, bl);
    ::encode(queue_len, bl);
    ::encode(cpu_load_avg, bl);
  }
  void decode(bufferlist::iterator &bl) {
    ::decode(auth, bl);
    ::decode(all, bl);
    ::decode(req_rate, bl);
    ::decode(cache_hit_rate, bl);
    ::decode(queue_len, bl);
    ::decode(cpu_load_avg, bl);
  }
};
WRITE_CLASS_ENCODER(mds_load_t)

inline ostream& operator<<( ostream& out, mds_load_t& load )
{
  return out << "mdsload<" << load.auth << "/" << load.all
             << ", req " << load.req_rate 
             << ", hr " << load.cache_hit_rate
             << ", qlen " << load.queue_len
	     << ", cpu " << load.cpu_load_avg
             << ">";
}

/*
inline mds_load_t& operator+=( mds_load_t& l, mds_load_t& r ) 
{
  l.root_pop += r.root_pop;
  l.req_rate += r.req_rate;
  l.queue_len += r.queue_len;
  return l;
}

inline mds_load_t operator/( mds_load_t& a, double d ) 
{
  mds_load_t r;
  r.root_pop = a.root_pop / d;
  r.req_rate = a.req_rate / d;
  r.queue_len = a.queue_len / d;
  return r;
}
*/


class load_spread_t {
public:
  static const int MAX = 4;
  int last[MAX];
  int p, n;
  DecayCounter count;

public:
  load_spread_t() : p(0), n(0) { 
    for (int i=0; i<MAX; i++) last[i] = -1;
  } 

  double hit(utime_t now, int who) {
    for (int i=0; i<n; i++)
      if (last[i] == who) 
	return count.get_last();

    // we're new(ish)
    last[p++] = who;
    if (n < MAX) n++;
    if (n == 1) return 0.0;

    if (p == MAX) p = 0;

    return count.hit(now);
  }
  double get(utime_t now) {
    return count.get(now);
  }
};



// ================================================================

//#define MDS_PIN_REPLICATED     1
//#define MDS_STATE_AUTH     (1<<0)

class MLock;
class SimpleLock;

class MDSCacheObject;

// -- authority delegation --
// directory authority types
//  >= 0 is the auth mds
#define CDIR_AUTH_PARENT   -1   // default
#define CDIR_AUTH_UNKNOWN  -2
#define CDIR_AUTH_DEFAULT   pair<int,int>(-1, -2)
#define CDIR_AUTH_UNDEF     pair<int,int>(-2, -2)
//#define CDIR_AUTH_ROOTINODE pair<int,int>( 0, -2)


/*
 * for metadata leases to clients
 */
struct ClientLease {
  int client;
  int mask;                 // CEPH_STAT_MASK_*
  MDSCacheObject *parent;

  utime_t ttl;
  xlist<ClientLease*>::item session_lease_item; // per-session list
  xlist<ClientLease*>::item lease_item;         // global list

  ClientLease(int c, MDSCacheObject *p) : 
    client(c), mask(0), parent(p),
    session_lease_item(this),
    lease_item(this) { }
};


// print hack
struct mdsco_db_line_prefix {
  MDSCacheObject *object;
  mdsco_db_line_prefix(MDSCacheObject *o) : object(o) {}
};
ostream& operator<<(ostream& out, mdsco_db_line_prefix o);

// printer
ostream& operator<<(ostream& out, MDSCacheObject &o);

class MDSCacheObjectInfo {
public:
  inodeno_t ino;
  dirfrag_t dirfrag;
  nstring dname;

  MDSCacheObjectInfo() : ino(0) {}

  void encode(bufferlist& bl) const {
    ::encode(ino, bl);
    ::encode(dirfrag, bl);
    ::encode(dname, bl);
  }
  void decode(bufferlist::iterator& p) {
    ::decode(ino, p);
    ::decode(dirfrag, p);
    ::decode(dname, p);
  }
};

WRITE_CLASS_ENCODER(MDSCacheObjectInfo)


class MDSCacheObject {
 public:
  // -- pins --
  const static int PIN_REPLICATED =  1000;
  const static int PIN_DIRTY      =  1001;
  const static int PIN_LOCK       = -1002;
  const static int PIN_REQUEST    = -1003;
  const static int PIN_WAITER     =  1004;
  const static int PIN_DIRTYSCATTERED = -1005;
  static const int PIN_AUTHPIN    =  1006;
  static const int PIN_PTRWAITER  = -1007;
  const static int PIN_TEMPEXPORTING = 1008;  // temp pin between encode_ and finish_export
  static const int PIN_CLIENTLEASE = 1009;

  const char *generic_pin_name(int p) {
    switch (p) {
    case PIN_REPLICATED: return "replicated";
    case PIN_DIRTY: return "dirty";
    case PIN_LOCK: return "lock";
    case PIN_REQUEST: return "request";
    case PIN_WAITER: return "waiter";
    case PIN_DIRTYSCATTERED: return "dirtyscattered";
    case PIN_AUTHPIN: return "authpin";
    case PIN_PTRWAITER: return "ptrwaiter";
    case PIN_TEMPEXPORTING: return "tempexporting";
    case PIN_CLIENTLEASE: return "clientlease";
    default: assert(0); return 0;
    }
  }

  // -- state --
  const static int STATE_AUTH      = (1<<30);
  const static int STATE_DIRTY     = (1<<29);
  const static int STATE_REJOINING = (1<<28);  // replica has not joined w/ primary copy

  // -- wait --
  const static int WAIT_SINGLEAUTH  = (1<<30);
  const static int WAIT_UNFREEZE    = (1<<29); // pka AUTHPINNABLE


  // ============================================
  // cons
 public:
  MDSCacheObject() :
    state(0), 
    ref(0),
    replica_nonce(0) {}
  virtual ~MDSCacheObject() {}

  // printing
  virtual void print(ostream& out) = 0;
  virtual ostream& print_db_line_prefix(ostream& out) { 
    return out << "mdscacheobject(" << this << ") "; 
  }
  
  // --------------------------------------------
  // state
 protected:
  unsigned state;     // state bits

 public:
  unsigned get_state() const { return state; }
  unsigned state_test(unsigned mask) const { return (state & mask); }
  void state_clear(unsigned mask) { state &= ~mask; }
  void state_set(unsigned mask) { state |= mask; }
  void state_reset(unsigned s) { state = s; }

  bool is_auth() const { return state_test(STATE_AUTH); }
  bool is_dirty() const { return state_test(STATE_DIRTY); }
  bool is_clean() const { return !is_dirty(); }
  bool is_rejoining() const { return state_test(STATE_REJOINING); }

  // --------------------------------------------
  // authority
  virtual pair<int,int> authority() = 0;
  bool is_ambiguous_auth() {
    return authority().second != CDIR_AUTH_UNKNOWN;
  }

  // --------------------------------------------
  // pins
protected:
  int      ref;       // reference count
#ifdef MDS_REF_SET
  multiset<int> ref_set;
#endif

 public:
  int get_num_ref() { return ref; }
  virtual const char *pin_name(int by) = 0;
  //bool is_pinned_by(int by) { return ref_set.count(by); }
  //multiset<int>& get_ref_set() { return ref_set; }

  virtual void last_put() {}
  virtual void bad_put(int by) {
#ifdef MDS_REF_SET
    assert(ref_set.count(by) > 0);
#endif
    assert(ref > 0);
  }
  void put(int by) {
#ifdef MDS_REF_SET
    if (ref == 0 || ref_set.count(by) == 0) {
#else
    if (ref == 0) {
#endif
      bad_put(by);
    } else {
      ref--;
#ifdef MDS_REF_SET
      ref_set.erase(ref_set.find(by));
      assert(ref == (int)ref_set.size());
#endif
      if (ref == 0)
	last_put();
    }
  }

  virtual void first_get() {}
  virtual void bad_get(int by) {
#ifdef MDS_REF_SET
    assert(by < 0 || ref_set.count(by) == 0);
#endif
    assert(0);
  }
  void get(int by) {
#ifdef MDS_REF_SET
    if (by >= 0 && ref_set.count(by)) {
      bad_get(by);
    } else {
#endif
      if (ref == 0) 
	first_get();
      ref++;
#ifdef MDS_REF_SET
      ref_set.insert(by);
      assert(ref == (int)ref_set.size());
    }
#endif
  }

  void print_pin_set(ostream& out) {
#ifdef MDS_REF_SET
    multiset<int>::iterator it = ref_set.begin();
    while (it != ref_set.end()) {
      out << " " << pin_name(*it);
      int last = *it;
      int c = 1;
      do {
	it++;
	if (it == ref_set.end()) break;
      } while (*it == last);
      if (c > 1)
	out << "*" << c;
    }
#endif
  }


  // --------------------------------------------
  // auth pins
  virtual bool can_auth_pin() = 0;
  virtual void auth_pin(void *who) = 0;
  virtual void auth_unpin(void *who) = 0;
  virtual bool is_frozen() = 0;


  // --------------------------------------------
  // replication (across mds cluster)
 protected:
  map<int,int> replica_map;   // [auth] mds -> nonce
  int          replica_nonce; // [replica] defined on replica

 public:
  bool is_replicated() { return !replica_map.empty(); }
  bool is_replica(int mds) { return replica_map.count(mds); }
  int num_replicas() { return replica_map.size(); }
  int add_replica(int mds) {
    if (replica_map.count(mds)) 
      return ++replica_map[mds];  // inc nonce
    if (replica_map.empty()) 
      get(PIN_REPLICATED);
    return replica_map[mds] = 1;
  }
  void add_replica(int mds, int nonce) {
    if (replica_map.empty()) 
      get(PIN_REPLICATED);
    replica_map[mds] = nonce;
  }
  int get_replica_nonce(int mds) {
    assert(replica_map.count(mds));
    return replica_map[mds];
  }
  void remove_replica(int mds) {
    assert(replica_map.count(mds));
    replica_map.erase(mds);
    if (replica_map.empty())
      put(PIN_REPLICATED);
  }
  void clear_replica_map() {
    if (!replica_map.empty())
      put(PIN_REPLICATED);
    replica_map.clear();
  }
  map<int,int>::iterator replicas_begin() { return replica_map.begin(); }
  map<int,int>::iterator replicas_end() { return replica_map.end(); }
  const map<int,int>& get_replicas() { return replica_map; }
  void list_replicas(set<int>& ls) {
    for (map<int,int>::const_iterator p = replica_map.begin();
	 p != replica_map.end();
	 ++p) 
      ls.insert(p->first);
  }

  int get_replica_nonce() { return replica_nonce;}
  void set_replica_nonce(int n) { replica_nonce = n; }


  // ---------------------------------------------
  // replicas (on clients)
 public:
  hash_map<int,ClientLease*> client_lease_map;

  ClientLease *get_client_lease(int c) {
    if (client_lease_map.count(c))
      return client_lease_map[c];
    return 0;
  }
  int get_client_lease_mask(int c) {
    ClientLease *l = get_client_lease(c);
    if (l) 
      return l->mask;
    else
      return 0;
  }

  ClientLease *add_client_lease(int c, int mask);
  int remove_client_lease(ClientLease *r, int mask, class Locker *locker);  // returns remaining mask (if any), and kicks locker eval_gathers
  

  // ---------------------------------------------
  // waiting
 protected:
  multimap<int, Context*>  waiting;

 public:
  bool is_waiter_for(int mask) {
    return waiting.count(mask) > 0;    // FIXME: not quite right.
  }
  virtual void add_waiter(int mask, Context *c) {
    if (waiting.empty())
      get(PIN_WAITER);
    waiting.insert(pair<int,Context*>(mask, c));
    pdout(10,g_conf.debug_mds) << (mdsco_db_line_prefix(this)) 
			       << "add_waiter " << hex << mask << dec << " " << c
			       << " on " << *this
			       << dendl;
    
  }
  virtual void take_waiting(int mask, list<Context*>& ls) {
    if (waiting.empty()) return;
    multimap<int,Context*>::iterator it = waiting.begin();
    while (it != waiting.end()) {
      if (it->first & mask) {
	ls.push_back(it->second);
	pdout(10,g_conf.debug_mds) << (mdsco_db_line_prefix(this))
				   << "take_waiting mask " << hex << mask << dec << " took " << it->second
				   << " tag " << it->first
				   << " on " << *this
				   << dendl;
	waiting.erase(it++);
      } else {
	pdout(10,g_conf.debug_mds) << "take_waiting mask " << hex << mask << dec << " SKIPPING " << it->second
				   << " tag " << it->first
				   << " on " << *this 
				   << dendl;
	it++;
      }
    }
    if (waiting.empty())
      put(PIN_WAITER);
  }
  void finish_waiting(int mask, int result = 0) {
    list<Context*> finished;
    take_waiting(mask, finished);
    finish_contexts(finished, result);
  }


  // ---------------------------------------------
  // locking
  // noop unless overloaded.
  virtual SimpleLock* get_lock(int type) { assert(0); return 0; }
  virtual void set_object_info(MDSCacheObjectInfo &info) { assert(0); }
  virtual void encode_lock_state(int type, bufferlist& bl) { assert(0); }
  virtual void decode_lock_state(int type, bufferlist& bl) { assert(0); }
  virtual void finish_lock_waiters(int type, int mask, int r=0) { assert(0); }
  virtual void add_lock_waiter(int type, int mask, Context *c) { assert(0); }
  virtual bool is_lock_waiting(int type, int mask) { assert(0); return false; }

  virtual void clear_dirty_scattered(int type) { assert(0); }
  virtual void finish_scatter_gather_update(int type) { }

  // ---------------------------------------------
  // ordering
  virtual bool is_lt(const MDSCacheObject *r) const = 0;
  struct ptr_lt {
    bool operator()(const MDSCacheObject* l, const MDSCacheObject* r) const {
      return l->is_lt(r);
    }
  };

};

inline ostream& operator<<(ostream& out, MDSCacheObject &o) {
  o.print(out);
  return out;
}

inline ostream& operator<<(ostream& out, const MDSCacheObjectInfo &info) {
  if (info.ino) return out << info.ino;
  if (info.dname.length()) return out << info.dirfrag << "/" << info.dname;
  return out << info.dirfrag;
}

inline ostream& operator<<(ostream& out, mdsco_db_line_prefix o) {
  o.object->print_db_line_prefix(out);
  return out;
}




#endif
