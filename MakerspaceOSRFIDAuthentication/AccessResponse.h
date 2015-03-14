#pragma once
class AccessResponse
{
public:
	AccessResponse();
	~AccessResponse();
	const char* Username;
	const char* ResponseMessage;
	const char* AccessAllowed;
	int TimeLimit;
};


