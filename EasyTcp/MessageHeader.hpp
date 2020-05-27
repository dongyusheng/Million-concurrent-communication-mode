#ifndef _MessageHeader_hpp_
#define _MessageHeader_hpp_

//消息的类型
enum CMD
{
	CMD_LOGIN,         //登录
	CMD_LOGIN_RESULT,  //登录结果
	CMD_LOGOUT,        //退出
	CMD_LOGOUT_RESULT, //退出结果
	CMD_NEW_USER_JOIN, //新的客户端加入
	CMD_ERROR          //错误
};

//数据报文的头部
struct DataHeader
{
	DataHeader() :cmd(CMD_ERROR), dataLength(sizeof(DataHeader)) {}
	short cmd;        //命令的类型
	short dataLength; //数据的长度
};

//登录消息体
struct Login :public DataHeader
{
	Login() {
		cmd = CMD_LOGIN;
		dataLength = sizeof(Login); //消息长度=消息头(父类)+消息体(子类)
	}
	char userName[32]; //账号
	char PassWord[32]; //密码
	char data[32];
};

//登录结果
struct LoginResult :public DataHeader
{
	LoginResult() :result(0) {
		cmd = CMD_LOGIN_RESULT;
		dataLength = sizeof(LoginResult);
	}
	int result; //登录的结果，0代表正常
	char data[92];
};

//退出消息体
struct Logout :public DataHeader
{
	Logout() {
		cmd = CMD_LOGOUT;
		dataLength = sizeof(Logout);
	}
	char userName[32]; //账号
};

//退出结果
struct LogoutResult :public DataHeader
{
	LogoutResult() :result(0) {
		cmd = CMD_LOGOUT_RESULT;
		dataLength = sizeof(LogoutResult);
	}
	int result; //退出的结果，0代表正常
};

//新的客户端加入，服务端给其他所有客户端发送此报文
struct NewUserJoin :public DataHeader
{
	NewUserJoin(int _cSocket = 0) :sock(_cSocket) {
		cmd = CMD_NEW_USER_JOIN;
		dataLength = sizeof(LogoutResult);
	}
	int sock; //新客户端的socket
};

#endif