#ifndef _MessageHeader_hpp_
#define _MessageHeader_hpp_

enum CMD
{
	CMD_LOGIN,   
	CMD_LOGIN_RESULT,
	CMD_LOGOUT,     
	CMD_LOGOUT_RESULT, 
	CMD_NEW_USER_JOIN,
	CMD_ERROR  
};

struct DataHeader
{
	DataHeader() :cmd(CMD_ERROR), dataLength(sizeof(DataHeader)) {}
	short cmd; 
	short dataLength;
};

struct Login :public DataHeader
{
	Login() {
		cmd = CMD_LOGIN;
		dataLength = sizeof(Login);
	}
	char userName[32];
	char PassWord[32];
	char data[32];
};

struct LoginResult :public DataHeader
{
	LoginResult() :result(0) {
		cmd = CMD_LOGIN_RESULT;
		dataLength = sizeof(LoginResult);
	}
	int result; 
	char data[92];
};

struct Logout :public DataHeader
{
	Logout() {
		cmd = CMD_LOGOUT;
		dataLength = sizeof(Logout);
	}
	char userName[32];
};

struct LogoutResult :public DataHeader
{
	LogoutResult() :result(0) {
		cmd = CMD_LOGOUT_RESULT;
		dataLength = sizeof(LogoutResult);
	}
	int result;
};

struct NewUserJoin :public DataHeader
{
	NewUserJoin(int _cSocket = 0) :sock(_cSocket) {
		cmd = CMD_NEW_USER_JOIN;
		dataLength = sizeof(LogoutResult);
	}
	int sock;
};

#endif