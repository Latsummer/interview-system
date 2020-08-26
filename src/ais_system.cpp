#include <fstream>
#include <stdio.h>
#include <unistd.h>
#include <unordered_map>
#include <jsoncpp/json/json.h>
using namespace std;

#include "tools.hpp"
#include "database.hpp"
#include "httplib.h"
#include "session.hpp"

#define REGUSER "insert into reg_userinfo(name, password, email) values(\"%s\", \"%s\", \"%s\")"
#define USERINFO "insert into stu_info(stu_name, stu_school, stu_major, stu_grade, stu_mobile) values(\"%s\", \"%s\", \"%s\", \"%s\", \"%s\")"
#define UPDATE_INTERVIEW_TIME "update stu_info set stu_choice_score=%d, stu_program_score=%d, stu_total_score=%d, stu_interview_time = \"%s\" where stu_id=%d"

#define START_TRANSATION "start transaction"
#define COMMIT "commit"

class AisSvr
{
public:
  AisSvr()
  {
    _svr_ip.clear();
    _svr_port = -1;

    _db = NULL;
    _db_ip.clear();
    _db_user.clear();
    _db_passwd.clear();
    _db_name.clear();
    _db_port = -1;

    _all_session = NULL;
  }

  int OnInit(const string& config_filename)
  {
    //不在构造函数中初始化，而提供接口初始化，原因是构造函数🈚️(没有)返回值
    if( !Load(config_filename) )//1. 加载配置文件
    {
      LOG(ERROR, "open config file failed") << endl;
      return -1;
    }
    LOG(INFO, "load config ok...") << endl;
    //2. 初始化数据库模块
    _db = new DataBaseSvr(_db_ip, _db_user, _db_passwd, _db_name, _db_port);
    if( !_db )
    {
      LOG(ERROR, "create database failed") << endl;
      return -2;
    }
    if( !_db->Connect2MySQL() )
    {
      LOG(ERROR, "connect database failed") << endl;
      return -3;
    }
    LOG(INFO, "connect database ok...") << endl;

    _all_session = new AllSessionInfo();
    if (!_all_session)
    {
      LOG(ERROR, "create all session info failed") << endl;
      return -4;
    }

    return 0;
  }

  void Start()
  {
    //1. 注册请求，用户名，密码，邮箱
    _http_svr.Post("/register", 
       [this](const httplib::Request& res, httplib::Response& resp){
            unordered_map<string, string> parm;
            UrlUtil::PraseBody(res.body, &parm);
            //1. 插入到数据库中
            //1.1 针对注册的信息，先插入注册信息表
            string name = parm["name"];
            string password = parm["password"];
            string email = parm["email"];

            string school = parm["school"];
            string major = parm["major"];
            string class_no = parm["class_no"];
            string phone_num = parm["phone_num"];
            
            _db->QuerySql(START_TRANSATION);//开启事务
            _db->QuerySql("savepoint 1");

            //1.2 组织插入语句
            char buf[1024];
            snprintf(buf, sizeof(buf) - 1, REGUSER, name.c_str(), password.c_str(), email.c_str());
            bool ret = _db->QuerySql(buf);
            if( !ret )
            {
              //1. 注册信息插入失败
              //1.1 结束事务
              _db->QuerySql(COMMIT);

              //1.2 返回应答
              Json::Value response_json;
              response_json["is_insert"] = false;

              Json::FastWriter writer;
              resp.body = writer.write(response_json);
              resp.set_header("Content-Type", "application/json");

              //1.3 结束该函数
              return;
            }

            memset(buf, '\0', sizeof(buf));
            snprintf(buf, sizeof(buf) - 1, USERINFO, name.c_str(), school.c_str(),
                major.c_str(), class_no.c_str(), phone_num.c_str());
            ret = _db->QuerySql(buf);
            if( !ret )
            {
              //1.回滚
              _db->QuerySql("rollback to 1");
              //1.1 提交事务
              _db->QuerySql(COMMIT);

              //1.2 组织应答
              Json::Value response_json;
              response_json["is_insert"] = false;

              Json::FastWriter writer;
              resp.body = writer.write(response_json);
              resp.set_header("Content-Type", "application/json");
              
              //1.3 结束该函数
              return;
            }

            _db->QuerySql(COMMIT);

            //2. 给浏览器响应一个应答，需要是json格式
            Json::Value response_json;
            response_json["is_insert"] = true;

            Json::FastWriter writer;
            resp.body = writer.write(response_json);
            resp.set_header("Content-Type", "application/json");
        });

    //2. 登录请求
    _http_svr.Post("/login", 
        [this](const httplib::Request& res, httplib::Response& resp){
            //1. 解析提交的内容
            unordered_map<string, string> parm;
            UrlUtil::PraseBody(res.body, &parm);
            Json::Value request_json;
            request_json["email"] = parm["email"];
            request_json["password"] = parm["password"];
            //2. 校验用户的邮箱和密码
            //2.1 如果校验失败，给浏览器返回false
            //2.2 如果校验成功，执行第三步
            
            //具体操作步骤需要再注册表中进行查询，先用提交上来的邮箱作为查询条件，
            //如果邮箱不存在，则登陆失败
            //如果邮箱存在，密码正确则登陆成功，密码不正确则登陆失败
            Json::Value response_json;
            bool ret = _db->QueryUserExist(request_json, &response_json);

            if( !ret )
              response_json["login_status"] = false;
            else
              response_json["login_status"] = true;
            
            //3. 面试预约界面请求(正常登录的情况下)
            //3.1 获取指定用户信息
            Json::Value user_info;
            _db->QueryOneStuInfo(response_json["stu_id"].asString(), &user_info);

            //3.2 生成sessionID
            Session sess(user_info);
            string session_id = sess.GetSessionId();
            _all_session->SetSessionValue(session_id, sess);

            string tmp = "JSESSIONID=";
            tmp += session_id;

            //返回sessionid，用来表示当前用户
            Json::FastWriter writer;
            resp.body = writer.write(response_json);
            resp.set_header("Set-Cookie", tmp.c_str());
            resp.set_header("Content-Type", "application/json");
        });

    _http_svr.Get("/interview", 
        [this](const httplib::Request& res, httplib::Response& resp){
          //根据请求头部中的sessionid，从_all_session中查询当前用户信息
          string session_id;

          GetSessionId(res, &session_id);

          
          Session sess;
          bool ret = _all_session->GetSessionValue(session_id, &sess);
          if (!ret)
          {
            //302 重定向
            //防止直接访问interview.html页面
            resp.set_redirect("/index.heml");
            return;
          }
          //2. 查询数据库，获取用户信息
          Json::Value response_json;
          ret = _db->QueryOneStuInfo(sess._user_info["stu_id"].asString(), &response_json);
          if (!ret)
          {
            //302 重定向
            //防止直接访问interview.html页面
            //resp.set_redirect("/index.heml");
            return;
          }

          //3. 组织应答
          Json::FastWriter writer;
          resp.body = writer.write(response_json);
          resp.set_header("Content-Type", "application/json");
        });

    //提交面试预约数据请求
    _http_svr.Post("/post_interview", 
        [this](const httplib::Request& res, httplib::Response& resp){
          //1. 从header获取sessionid
          string session_id;
          GetSessionId(res, &session_id);

          Session sess;
          _all_session->GetSessionValue(session_id, &sess);

          //2. 获取正文当中提交的信息，进行切割，得到value
          unordered_map<string, string> pram;
          pram.clear();
          UrlUtil::PraseBody(res.body, &pram);
          string choice_score = pram["choice_score"];
          string program_score = pram["program_score"];
          string total_score = pram["total_score"];
          string interview_time = pram["interview_time"];

          //3. 组织更新sql语句
          char sql[1024] = {0};
          snprintf(sql, sizeof(sql) - 1, UPDATE_INTERVIEW_TIME, 
                  atoi(choice_score.c_str()), atoi(program_score.c_str()), atoi(total_score.c_str()),
                  interview_time.c_str(), sess._user_info["stu_id"].asInt());

          //4. 调用执行sql语句的函数
          Json::Value response_json;
          bool ret = _db->QuerySql(sql);
          if (!ret)
            response_json["is_modify"] = false;
          else
            response_json["is_modify"] = true;

          //5. 组织应答
          Json::FastWriter writer;
          resp.body = writer.write(response_json);
          resp.set_header("Content-Type", "application/json");

        });
    
    //侦听
    _http_svr.set_mount_point("/", "./www");
    LOG(INFO, "start server... [ip:") << _svr_ip << "] [port:" << _svr_port << "]" << endl;
    _http_svr.listen(_svr_ip.c_str(), _svr_port);
  }

  void GetSessionId(httplib::Request res, string* session_id)
  {
    string session = res.get_header_value("Cookie");
    *(session_id) = session.substr(81);
  }

  bool Load(const string& config_filename)//打开配置文件
  {
    ifstream file(config_filename.c_str());
    if(!file.is_open())
    {
      LOG(ERROR, "open file failed") << endl;
      return false;
    }

    //正常打开文件了
    string line;
    vector<string> output;
    while( getline(file, line) )
    {
      output.clear();

      //解析内容
      StringTools::Split(line, "=", &output);
      if(output[0] == "svr_ip")
      {
        if( output[1].empty() )
        {
          LOG(ERROR, "ip is empty");
          return false;
        }
        _svr_ip = output[1];
      }
      else if(output[0] == "svr_port")
      {
        if( output[1].empty() ) 
        {
          LOG(ERROR, "port is empty");
          return false;
        }
        _svr_port = atoi(output[1].c_str());
      }
      else if(output[0] == "db_ip")
      {
        if( output[1].empty() ) 
        {
          LOG(ERROR, "db_ip is empty");
          return false;
        }
        _db_ip = output[1];
      }
      else if(output[0] == "db_user")
      {
        if( output[1].empty() ) 
        {
          LOG(ERROR, "db_user is empty");
          return false;
        }
        _db_user = output[1];
      }
      else if(output[0] == "db_passwd")
      {
        if( output[1].empty() ) 
        {
          LOG(ERROR, "db_passwd is empty");
          return false;
        }
        _db_passwd = output[1];
      }
      else if(output[0] == "db_name")
      {
        if( output[1].empty() ) 
        {
          LOG(ERROR, "db_name is empty");
          return false;
        }
        _db_name = output[1];
      }
      else if(output[0] == "db_port")
      {
        if( output[1].empty() ) 
        {
          LOG(ERROR, "db_port is empty");
          return false;
        }
        _db_port = atoi(output[1].c_str());
      }
    }

    return true;
  }

private:
  string _svr_ip;//服务端监听的IP地址
  uint16_t _svr_port;//服务端监听的端口

  //数据库类成员
  DataBaseSvr* _db;
  string _db_ip;
  string _db_user;
  string _db_passwd;
  string _db_name;
  uint16_t _db_port;

  AllSessionInfo* _all_session;//所有登录用户的sessionid

  httplib::Server _http_svr;
};

int main()
{
  AisSvr as;
  int ret = as.OnInit("./config_ais.cfg");
  if( ret < 0 )
  {
    LOG(ERROR, "Init server failed") << endl;
    return -1;
  }
  as.Start();
  return 0;
}
