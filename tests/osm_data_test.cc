#include <iostream>
#include <sstream>
#include <fstream>
#include <string>
#include <cstdlib>
#include <cstring>
#include <future>
#include "ycsb/ycsb-c.h"
#include "nvm_alloc.h"
#include "common_time.h"
#include "../src/combotree_config.h"
#include "fast-fair/btree.h"
#include "learnindex/pgm_index_dynamic.hpp"
#include "learnindex/rmi.h"
#include "xindex/xindex_impl.h"
#include "alex/alex.h"
#include "stx/btree_map.h"
#include "../src/learn_group.h"
#include "random.h"

#define LOAD_NUM 200000000
#define WORK_NUM 1000000
#define MAX_LAT 1000000000

using namespace std;
using std::random_shuffle;
using FastFair::btree;
using xindex::XIndex;

// 保存初始的加载数据
vector<pair<uint64_t, uint64_t>> load_;
// 保存生成的负载，pair的第一个值为0为read，否则为insert
vector<pair<uint64_t, uint64_t>> work_;
// 读比例（0~10）
int read_prop = 0;
// copy from ycsb
class KvDB {
 public:
  typedef std::pair<uint64_t, uint64_t> KVPair;
  static const int kOK = 0;
  static const int kErrorNoData = 1;
  static const int kErrorConflict = 2;
  ///
  /// Initializes any state for accessing this DB.
  /// Called once per DB client (thread); there is a single DB instance globally.
  ///
  virtual void Init() { }
  ///
  /// Clears any state for accessing this DB.
  /// Called once per DB client (thread); there is a single DB instance globally.
  ///
  virtual void Close() { }
  virtual void Info() { }
  virtual void Begin_trans() {}
  virtual int Put(uint64_t key, uint64_t value) = 0;
  virtual int Get(uint64_t key, uint64_t &value) = 0;
  virtual int Update(uint64_t key, uint64_t value) = 0;
  virtual int Scan(uint64_t start_key, int len, std::vector<std::pair<uint64_t, uint64_t>>& results) = 0;
  virtual void PrintStatic() {}
  // virtual int Delete(const std::string &table, const std::string &key) = 0;
  
  virtual ~KvDB() { }
};

class FastFairDb : public KvDB {
public:
    FastFairDb(): tree_(nullptr) {}
    FastFairDb(btree *tree): tree_(tree) {}
    virtual ~FastFairDb() {}
    void Init()
    {
      NVM::data_init(); 
      tree_ = new btree();
    }

    void Info()
    {
      NVM::show_stat();
      tree_->PrintInfo();
    }

    void Close() { 

    }
    int Put(uint64_t key, uint64_t value) 
    {
        tree_->btree_insert(key, (char *)value);
        return 1;
    }
    int Get(uint64_t key, uint64_t &value)
    {
        value = (uint64_t)tree_->btree_search(key);
        return 1;
    }
    int Update(uint64_t key, uint64_t value) {
        tree_->btree_delete(key);
        tree_->btree_insert(key, (char *)value);
        return 1;
    }
    int Scan(uint64_t start_key, int len, std::vector<std::pair<uint64_t, uint64_t>>& results) 
    {
        tree_->btree_search_range(start_key, UINT64_MAX, results, len);
        return 1;
    }
private:
    btree *tree_;
};

class PGMDynamicDb : public KvDB {
#ifdef USE_MEM
  using PGMType = pgm::PGMIndex<uint64_t>;
#else
  using PGMType = PGM_OLD_NVM::PGMIndex<uint64_t>;
#endif
  typedef pgm::DynamicPGMIndex<uint64_t, char *, PGMType> DynamicPGM;
public:
  PGMDynamicDb(): pgm_(nullptr) {}
  PGMDynamicDb(DynamicPGM *pgm): pgm_(pgm) {}
  virtual ~PGMDynamicDb() {
    delete pgm_;
  }

  void Init()
  {
    NVM::data_init();
    pgm_ = new DynamicPGM();
  }
  void Info()
  {
    NVM::show_stat();
  }

  void Begin_trans()
  {
    pgm_->trans_to_read();
  }

  int Put(uint64_t key, uint64_t value) 
  {
    pgm_->insert(key, (char *)value);
    return 1;
  }
  int Get(uint64_t key, uint64_t &value)
  {
      auto it = pgm_->find(key);
      value = (uint64_t)it->second;
      return 1;
  }
  int Update(uint64_t key, uint64_t value) {
      pgm_->insert(key, (char *)value);
      return 1;
  }
  int Scan(uint64_t start_key, int len, std::vector<std::pair<uint64_t, uint64_t>>& results) 
  {
    int scan_count = 0;
    auto it = pgm_->find(start_key);
    while(it != pgm_->end() && scan_count < len) {
      results.push_back({it->first, (uint64_t)it->second});
      ++it;
      scan_count ++;
    }  
    return 1;
  }
private:
  DynamicPGM *pgm_;
};

class XIndexDb : public KvDB  {
  static const int init_num = 10000;
  static const int bg_num = 1;
  static const int work_num = 1;
  typedef RMI::Key_64 index_key_t;
  typedef xindex::XIndex<index_key_t, uint64_t> xindex_t;
public:
  XIndexDb(): xindex_(nullptr) {}
  XIndexDb(xindex_t *xindex): xindex_(xindex) {}
  virtual ~XIndexDb() {
    delete xindex_;
  }

  void Init()
  {
    NVM::data_init();
    prepare_xindex(init_num, bg_num, work_num);
  }
  void Info()
  {
    NVM::show_stat();
  }
  int Put(uint64_t key, uint64_t value) 
  {
    // pgm_->insert(key, (char *)value);
    xindex_->put(index_key_t(key), value >> 4, 0);
    return 1;
  }
  int Get(uint64_t key, uint64_t &value)
  {
      xindex_->get(index_key_t(key), value, 0);
      return 1;
  }
  int Update(uint64_t key, uint64_t value) {
      xindex_->put(index_key_t(key), value >> 4, 0);
      return 1;
  }
  int Scan(uint64_t start_key, int len, std::vector<std::pair<uint64_t, uint64_t>>& results) 
  {
    std::vector<std::pair<index_key_t, uint64_t>> tmpresults;
    xindex_->scan(index_key_t(start_key), len, tmpresults, 0);
    return 1;
  } 
private:
  inline void 
  prepare_xindex(size_t init_size, int fg_n, int bg_n) {
    // prepare data
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int64_t> rand_int64(
        0, std::numeric_limits<int64_t>::max());
    std::vector<index_key_t> initial_keys;
    initial_keys.reserve(init_size);
    for (size_t i = 0; i < init_size; ++i) {
      initial_keys.push_back(index_key_t(rand_int64(gen)));
    }
    // initilize XIndex (sort keys first)
    std::sort(initial_keys.begin(), initial_keys.end());
    std::vector<uint64_t> vals(initial_keys.size(), 1);
    xindex_ = new xindex_t(initial_keys, vals, fg_n, bg_n);
  }
  xindex_t *xindex_;
};

class AlexDB : public KvDB  {
  typedef uint64_t KEY_TYPE;
  typedef uint64_t PAYLOAD_TYPE;
#ifdef USE_MEM
  using Alloc = std::allocator<std::pair<KEY_TYPE, PAYLOAD_TYPE>>;
#else
  using Alloc = NVM::allocator<std::pair<KEY_TYPE, PAYLOAD_TYPE>>;
#endif
  typedef alex::Alex<KEY_TYPE, PAYLOAD_TYPE, alex::AlexCompare, Alloc> alex_t;
public:
  AlexDB(): alex_(nullptr){}
  AlexDB(alex_t *alex): alex_(alex) {}
  virtual ~AlexDB() {
    delete alex_;
  }

  void Init()
  {
    NVM::data_init();
    alex_ = new alex_t();
  }

  void Info()
  {
    NVM::show_stat();
    //加载完数据的树高
    alex_->PrintInfo();
  }

  int Put(uint64_t key, uint64_t value) 
  {
    alex_->insert(key, value);
    return 1;
  }
  int Get(uint64_t key, uint64_t &value)
  {
    value = *(alex_->get_payload(key));
    // assert(value == key);
    return 1;
  }
  int Update(uint64_t key, uint64_t value) {
      uint64_t *addrs = (alex_->get_payload(key));
      *addrs = value;
      NVM::Mem_persist(addrs, sizeof(uint64_t));
      return 1;
  }
  int Scan(uint64_t start_key, int len, std::vector<std::pair<uint64_t, uint64_t>>& results) 
  {
    auto it = alex_->lower_bound(start_key);
    int num_entries = 0;
    while (num_entries < len && it != alex_->end()) {
      results.push_back({it.key(), it.payload()});
      num_entries ++;
      it++;
    }
    return 1;
  } 
  void PrintStatic() {
    // std::cerr << "Alevel average cost: " << Common::timers["ABLevel_times"].avg_latency() << std::endl;
    // std::cerr << "Clevel average cost: " << Common::timers["Clevel_times"].avg_latency() << std::endl;
  }
private:
  alex_t *alex_;
};

class LearnGroupDB : public KvDB  {
  static const size_t init_num = 1000;
  void Prepare() {
    std::vector<std::pair<uint64_t,uint64_t>> initial_kv;
    combotree::Random rnd(0, UINT64_MAX - 1);
    initial_kv.push_back({0, UINT64_MAX});
    for (uint64_t i = 0; i < init_num - 1; ++i) {
        uint64_t key = rnd.Next();
        uint64_t value = rnd.Next();
        initial_kv.push_back({key, value});
    }
    sort(initial_kv.begin(), initial_kv.end());
    root_->Load(initial_kv);
  }
public:
  LearnGroupDB(): root_(nullptr) {}
  LearnGroupDB(combotree::RootModel *root): root_(root) {}
  virtual ~LearnGroupDB() {
    delete root_;
  }

  void Init()
  {
    NVM::data_init();
    root_ = new combotree::RootModel();
    Prepare();
  }

  void Info()
  {
    NVM::show_stat();
    root_->Info();
  }

  int Put(uint64_t key, uint64_t value) 
  {
    // alex_->insert(key, value);
    root_->Put(key, value);
    return 1;
  }
  int Get(uint64_t key, uint64_t &value)
  {
      root_->Get(key, value);
      return 1;
  }
  int Update(uint64_t key, uint64_t value) {
      root_->Update(key, value);
      return 1;
  }
  int Scan(uint64_t start_key, int len, std::vector<std::pair<uint64_t, uint64_t>>& results) 
  {
    combotree::RootModel::Iter it(root_, start_key);
    int num_entries = 0;
    while (num_entries < len && !it.end()) {
      results.push_back({it.key(), it.value()});
      num_entries ++;
      it.next();
    }
    return 1;
  } 
  void PrintStatic() {
      // std::cerr << "Alevel average cost: " << Common::timers["ALevel_times"].avg_latency();
      // std::cerr << ",Blevel average cost: " << Common::timers["BLevel_times"].avg_latency();
      // std::cerr << ",Clevel average cost: " << Common::timers["CLevel_times"].avg_latency() << std::endl;
      // Common::timers["ALevel_times"].clear();
      // Common::timers["BLevel_times"].clear();
      // Common::timers["CLevel_times"].clear();
  }
private:
  combotree::RootModel *root_;
};

int load_data(){
  string data_path = "/home/wjy/asia_200m_outfile.csv";
  ifstream inf;
  //使key为uint64_t类型
  const int mul = 1e7;
  const int add = 13 * mul;
  inf.open(data_path);
  if(!inf){
    cerr << "can not open this file" << endl;
    return 0;
  }
  string line;
  while(getline(inf,line)){
    istringstream sin(line);
    double lat_d, lon_d;
    sin >> lat_d;
    sin >> lon_d;
    uint64_t lat = (uint64_t)(lat_d * mul + add) << 32 + (uint64_t)(lon_d * mul);
    uint64_t lon = (uint64_t)(lon_d * mul);
    load_.push_back(make_pair(lat, lon));
  }
  inf.close();
  cout << "load data:" << load_.size() << endl;
  // cout << "part of load_" << endl;
  // for(int i = 0;i<5;i++){
  //   cout << load_[i].first << '\t' << load_[i].second << endl;
  // }
  return 0;
}

//生成负载
int gen_data(){
  int read_num = WORK_NUM * read_prop / 10;
  int insert_num = WORK_NUM - read_num;
  uint64_t max_64 = pow(2, 64) - 1;
  for (int i = 0; i < read_num ;i++){
    uint64_t tmp_pos = (uint64_t) (((double) rand() / RAND_MAX) * LOAD_NUM);
    work_.push_back(make_pair(load_[tmp_pos].first,0));
  }
  for (int i = 0; i < insert_num;i++){
    uint64_t tmp_key = (uint64_t) (((double) rand() / RAND_MAX) * max_64);
    uint64_t tmp_value = rand()+1;//加一为了防止是0
    work_.push_back(make_pair(tmp_key, tmp_value));
  }
  random_shuffle(work_.begin(), work_.end());
  cout << "generate work data :" << work_.size() << endl;
  // for(int i = 0;i<5;i++){
  //   cout << work_[i].first << '\t' << work_[i].second << endl;
  // }
  return 0;
}

int load_db(KvDB * db){
  // 存入db
  cout << "loading..." << endl;
  for (int i = 0; i < load_.size();i++){
    db->Put(load_[i].first, load_[i].second);
  }
  cout << "loaded in db" << endl;
  return 0;
}

int run_test(KvDB * db,string dbname){
  db->Begin_trans();
  utils::Timer<double> timer;
  timer.Start();
  for (int i = 0; i < WORK_NUM;i++){
    if(work_[i].second == 0){
      uint64_t tmp_value;
      db->Get(work_[i].first, tmp_value);
      //cout << "get error" << endl;
    }else{
      db->Put(work_[i].first, work_[i].second);
      //cout << "put error" << endl;
    }
  }
  double duration = timer.End();
  cout << "# throughput (KTPS)" << endl;
  cout << dbname << '\t' << "load num:" << LOAD_NUM << '\t' << "OP num:" << WORK_NUM << '\t';
  cout << WORK_NUM / duration / 1000 << endl << endl;
  return 0;
}

void UsageMessage(const char *command) {
  cout << "Usage: " << command << " [options]" << endl;
  cout << "Options:" << endl;
  cout << "  -db dbname: specify the name of the DB to use (default: basic)" << endl;
  cout << "  -read : read proption(0~10)" << endl;
}

inline bool StrStartWith(const char *str, const char *pre) {
  return strncmp(str, pre, strlen(pre)) == 0;
}

string ParseCommandLine(int argc, const char *argv[]) {
  string dbname;
  int argindex = 1;
  while (argindex < argc && StrStartWith(argv[argindex], "-")) {
    if (strcmp(argv[argindex], "-db") == 0) {
      argindex++;
      if (argindex >= argc) {
        UsageMessage(argv[0]);
        exit(0);
      }
      dbname = argv[argindex];
      argindex++;
    }else if (strcmp(argv[argindex], "-read") == 0) {
      argindex++;
      if (argindex >= argc) {
        UsageMessage(argv[0]);
        exit(0);
      }
      read_prop = std::stoi(argv[argindex]);
      argindex++;
    } else {
      cout << "Unknown option '" << argv[argindex] << "'" << endl;
      exit(0);
    }
  }
  if (argindex == 1 || argindex != argc) {
    UsageMessage(argv[0]);
    exit(0);
  }
  return dbname;
}

int main(int argc, const char *argv[]){
    NVM::env_init();
    KvDB *db = nullptr;
    string dbName = ParseCommandLine(argc, argv);
    //加载数据
    load_.reserve(LOAD_NUM);
    work_.reserve(WORK_NUM);
    load_data();
    //生成负载
    gen_data();
    std::cout << "osm data test:" << dbName << std::endl;
    if(dbName == "fastfair") {
      db = new FastFairDb();
    } else if(dbName == "pgm") {
      db = new PGMDynamicDb();
    } else if(dbName == "xindex") {
      db = new XIndexDb();
    } else if(dbName == "alex") {
      db = new AlexDB();
    }else if(dbName == "learngroup"){
      db = new LearnGroupDB();
    }else if(dbName == "all"){
      db = new FastFairDb();
      db->Init();
      load_db(db);
      db->Info();
      dbName = "fastfair";
      run_test(db,dbName);
      delete db;
      NVM::env_exit();

      NVM::env_init();
      db = new PGMDynamicDb();
      db->Init();
      load_db(db);
      db->Info();
      dbName = "pgm";
      run_test(db,dbName);
      delete db;
      NVM::env_exit();
      NVM::env_init();

      db = new XIndexDb();
      db->Init();
      load_db(db);
      db->Info();
      dbName = "xindex";
      run_test(db,dbName);
      delete db;
      NVM::env_exit();
      NVM::env_init();

      db = new AlexDB();
      db->Init();
      load_db(db);
      db->Info();
      dbName = "alex";
      run_test(db,dbName);
      delete db;
      NVM::env_exit();
      return 0;

      db = new LearnGroupDB();
      db->Init();
      load_db(db);
      db->Info();
      dbName = "learngroup";
      run_test(db,dbName);
      delete db;
      NVM::env_exit();
    }
    db->Init();
    load_db(db);
    db->Info();
    run_test(db,dbName);
    delete db;
    NVM::env_exit();
    return 0;
}

