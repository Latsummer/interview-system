#pragma once
#include <string>
#include <jsoncpp/json/json.h>
#include <openssl/md5.h>
#include <unordered_map>
#include <pthread.h>
#include "tools.hpp"
using namespace std;
//1. 计算sessionid
//2. 保存登录用户sessionid和其对应的用户信息

class Session
{

  public:

    Session()
    {}
    Session(Json::Value& user_info)
    {
      _origin_str.clear();
      _user_info = user_info;

      _origin_str += to_string(_user_info["stu_id"].asInt());
      _origin_str += _user_info["stu_name"].asString();
      _origin_str += _user_info["stu_interview_time"].asString();

      
    }

    ~Session()
    {}

    bool SumMd5()
    {
      //生成32位MD5值，当作sessionid

      //1. 定义MD5操作句柄 & 进行初始化
      MD5_CTX ctx;
      MD5_Init(&ctx);

      //2. 计算MD5值
      int ret = MD5_Update(&ctx, _origin_str.c_str(), _origin_str.size());
      if (ret != 1)
      {
        LOG(ERROR, "MD5_Update failed") << endl;
        return false;
      }

      //3. 获取计算完成的MD5值
      unsigned char md5[16] = {0};
      ret = MD5_Final(md5, &ctx);
      if (ret != 1)
      {
        LOG(ERROR, "MD5_Final failed") << endl;
        return false;
      }

      //32位的字符串就是计算出来的sessionid
      char tmp[2] = {0};
      char buf[32] = {0};
      for(int i = 0; i < sizeof(md5) / sizeof(md5[0]); i++)
      {
        sprintf(tmp, "%02x", md5[i]);
        strncat(buf, tmp, 2);
      }
      
      LOG(INFO, buf) << endl;
      _session_id = buf;
      return true;
    }

    string& GetSessionId()
    {
      SumMd5();
      return _session_id;
    }

  //private:
    string _session_id;//保存session_id
    string _origin_str;//原始的串，用来生成session_id
    Json::Value _user_info;//原始串内容：stu_id, stu_name, stu_interview_time
};

class AllSessionInfo
{
public:
  AllSessionInfo()
  {
    _session_map.clear();
    pthread_mutex_init(&_map_lock, NULL);
  }

  ~AllSessionInfo()
  {
    _session_map.clear();
    pthread_mutex_destroy(&_map_lock);
  }

  //Set Session
  bool SetSessionValue(string& session_id, Session& session_info)
  {
    pthread_mutex_lock(&_map_lock);

    _session_map.insert(make_pair(session_id, session_info));

    pthread_mutex_unlock(&_map_lock);
    return true;
  }

  //Get Session
  bool GetSessionValue(string& session_id, Session* session_info)
  {
    pthread_mutex_lock(&_map_lock);

    auto iter = _session_map.find(session_id);
    if (iter == _session_map.end())
    {
      pthread_mutex_unlock(&_map_lock);
      return false;
    }

    *session_info = iter->second;

    pthread_mutex_unlock(&_map_lock);
    return true;
  }

private:
  //key：sessionid，value：session
  unordered_map<string, Session> _session_map;
  pthread_mutex_t _map_lock;
};