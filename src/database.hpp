#pragma once
#include <string>
#include <iostream>
#include <unistd.h>
#include <mysql/mysql.h>
#include <jsoncpp/json/json.h>
using namespace std;

#include "tools.hpp"

#define JUDGEVALUE(p) ((p != NULL ) ? p : "")
#define JUDGEUSER "select * from reg_userinfo where email='%s'"
#define GET_ONE_STU_INFO "select * from stu_info where stu_id = \"%s\""

class DataBaseSvr//数据库类
{
  
  public:
    DataBaseSvr(string& host, string& user, string& passwd, string& db, uint16_t port)
    {
      _host = host;
      _user = user;
      _passwd = passwd;
      _db = db;
      _port = port;

      mysql_init(&_mysql);
    }

    ~DataBaseSvr()
    {
      mysql_close(&_mysql);
    }
    
    bool Connect2MySQL()
    {
      //客户端操作句柄，MySQL服务端所在主机IP，用户，密码，数据库，端口，NULL，
      //mysql_real_connect(&_mysql, _host, _user, _passwd, _db, _port, NULL, );
      if( !mysql_real_connect(&_mysql, _host.c_str(), _user.c_str(), _passwd.c_str(),
            _db.c_str(), _port, NULL, CLIENT_FOUND_ROWS) )
      {
        LOG(ERROR, "connect database failed, ") << mysql_errno(&_mysql) << endl;
        return false;
      }

      return true;
    }

    //数据库操作数据的接口
  bool QuerySql(const string& sql)
  {
    //设置编码格式
    mysql_query(&_mysql, "set names utf8");

    //插入数据
    if( mysql_query(&_mysql, sql.c_str()) )
    {
      //插入失败
      LOG(ERROR, "exec sql: ") << sql << "failed!" << endl;
      return false;
    }
    return true;
  }

  bool QueryUserExist(Json::Value& request_json, Json::Value* result)//数据库查询
  {
    mysql_query(&_mysql, "set names utf8");
    //json对象转换string时，需要调用asString接口
    string email = request_json["email"].asString();
    string password = request_json["password"].asString();


    //1. 组织查询语句
    char sql[1024] = {0};
    snprintf(sql, sizeof(sql) - 1, JUDGEUSER, email.c_str());

    //1.1 查询
    if( mysql_query(&_mysql, sql) )
    {
      //sql语句没有执行成功
      LOG(ERROR, "database error in query") << mysql_error(&_mysql) << endl;
      return false;
    }
    //获取查询结果集，结果集通过返回值返回
    MYSQL_RES* res = mysql_store_result(&_mysql);
    if( !res )
    {
      //获取结果集没有成功
      LOG(ERROR, "Get result failed: ") << mysql_error(&_mysql) << endl;
      return false;
    }

    //2. 判断
    //2.1 判断结果集中有没有数据（获取到了结果集，但是结果集中没有数据）
    int row_num = mysql_num_rows(res);
    if( row_num <= 0 )
    {
      //返回给浏览器的内容是当前用户不存在
      //(*response_json)["is_exist"] = false;
      mysql_free_result(res);
      return false;
    }
    //(*response_json)["is_exist"] = true;

    //结果集有内容，判断密码是否正确
    MYSQL_ROW row = mysql_fetch_row(res);//该函数作用是：获取结果集中的一行内容
    //返回的row相当于一个数组
    //row[0]---> id, row[1]---> name, row[2]---> password
    //这样的获取方式有一个弊端：row[x]本身就没有值，此时强行获取再赋值给string对象，程序就会崩溃
    //cout << row[2] << endl;
    string tmp_password = JUDGEVALUE(row[2]);

    if( strcmp(tmp_password.c_str(), password.c_str()) != 0)
    {
      //(*response_json)["is_password"] = true;
      LOG(ERROR, "用户邮箱为： ") << email << " 密码错误,密码为：" << tmp_password << endl;
      mysql_free_result(res);
      return false;
    }
    //else
   // {
   // (*response_json)["is_password"] = false;

    (*result)["stu_id"] = row[0];
    mysql_free_result(res);
    return true;

  }

  bool QueryOneStuInfo(string user_id, Json::Value* result)
  {
    mysql_query(&_mysql, "set names utf8");
    char sql[1024] = {0};
    snprintf(sql, sizeof(sql) - 1, GET_ONE_STU_INFO, user_id.c_str());

    if( mysql_query(&_mysql, sql) )
    {
      //sql语句没有执行成功
      LOG(ERROR, "database error in query") << mysql_error(&_mysql) << endl;
      return false;
    }
    
    //获取查询结果集，结果集通过返回值返回
    MYSQL_RES* res = mysql_store_result(&_mysql);
    if( !res )
    {
      //获取结果集没有成功
      LOG(ERROR, "Get result failed: ") << mysql_error(&_mysql) << endl;
      return false;
    }

    //2. 判断
    //2.1 判断结果集中有没有数据（获取到了结果集，但是结果集中没有数据）
    int row_num = mysql_num_rows(res);

    if( row_num <= 0 )
    {
      //返回给浏览器的内容是当前用户不存在
      //(*response_json)["is_exist"] = false;
      mysql_free_result(res);

      return false;
    }



    //打包json串
    MYSQL_ROW row = mysql_fetch_row(res);

    (*result)["stu_id"] = atoi(row[0]);
    (*result)["stu_name"] = JUDGEVALUE(row[1]);
    (*result)["stu_choice_score"] = JUDGEVALUE(row[6]);
    (*result)["stu_program_score"] = JUDGEVALUE(row[7]);
    (*result)["stu_total_score"] = JUDGEVALUE(row[8]);
    (*result)["stu_speculative_score"] = JUDGEVALUE(row[10]);
    (*result)["stu_code_score"] = JUDGEVALUE(row[11]);
    (*result)["stu_think_score"] = JUDGEVALUE(row[12]);
    (*result)["stu_expression_score"] = JUDGEVALUE(row[13]);
    (*result)["stu_interview_score"] = JUDGEVALUE(row[14]);
    (*result)["stu_interview_techer"] = JUDGEVALUE(row[15]);
    (*result)["stu_techer_suggest"] = JUDGEVALUE(row[16]);
    (*result)["stu_interview_time"] = JUDGEVALUE(row[9]);

    mysql_free_result(res);
    return true;
  }

  private:
    //MySQL就是客户端的操作句柄
    MYSQL _mysql;
    string _host;
    string _user;
    string _passwd;
    string _db;
    uint16_t _port;
};
