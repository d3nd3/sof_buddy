#if !defined(PACKET_INC)
#define PACKET_INC

#include <vector>
#include <string>
using namespace std;

class InPacket;
class OutPacket
{
	int InitialSize;
	int CheckPoint;
	friend class InPacket;
	vector<unsigned char> Data;
	int BitPos;
public:
	OutPacket(int initialSize);
	bool SetCheckPoint();
	void PutBits(const void *x,int NumBits);
	void PutBool(bool t) {PutBits(&t,1);}
	int SizeBytes() const {return Data.size();}
	int SizeBits() const {return BitPos;}
	int RemainingBytes() const {return InitialSize-(BitPos+7)/8;}
	const unsigned char *Buffer() const {return &Data[0];}
	void PutBytes(const void *x,int NumBytes) {PutBits(x,NumBytes*8);}
	void Concat(const OutPacket &c) {PutBits(&c.Data[0],c.BitPos);}
	void PutString(const string &x) {PutString(x.c_str());}
	void PutString(const char *x);
	template <class T> void Put(T &x) {PutBytes(&x,sizeof(T));}
	template <class T> void PutVRS(const T &x)
	{
		if (!x)
		{
			PutBool(false);
			return;
		}
		PutBool(true);
		T xx=x;
		if (xx<0)
		{
			PutBool(true);
			xx=-xx;
		}
		else
			PutBool(false);
		int blocksize=sizeof(T)*2;
		T m;
		int num;
		for (num=0,m=1<<blocksize;m&&xx>=m;m<<=blocksize,num++)
			;
		assert(num<4);
		PutBits(&num,2);
		PutBits(&xx,(num+1)*blocksize);
	}
	template <class T> void PutVRU(const T &x)
	{
		static const int zero=0;
		static const int one=1;
		assert(x>=0);
		if (!x)
		{
			PutBool(false);
			return;
		}
		PutBool(true);
		int blocksize=sizeof(T)*2;
		T m;
		int num;
		for (num=0,m=1<<blocksize;m&&x>=m;m<<=blocksize,num++)
			;
		assert(num<4);
		PutBits(&num,2);
		PutBits(&x,(num+1)*blocksize);
	}
	template <class T> void Delta(const T &x,const T &old) 
	{
		if (x==old)
			PutBool(false);
		else
		{
			PutBool(true);
			Put(x);
		}
	}
	template <class T> void DeltaVRS(const T &x,const T &old) 
	{
		if (x==old)
			PutBool(false);
		else
		{
			PutBool(true);
			PutVRS(x);
		}
	}
	template <class T> void DeltaVRU(const T &x,const T &old) 
	{
		if (x==old)
			PutBool(false);
		else
		{
			PutBool(true);
			PutVRU(x);
		}
	}
	template <class T> void PutBU(const T &x,int bits) 
	{
		if (!bits)
			return;
		PutBits(&x,bits);
	}
	template <class T> void DeltaBU(const T &x,const T &old,int bits) 
	{
		if (!bits)
			return;
		if (x==old)
			PutBool(false);
		else
		{
			PutBool(true);
			PutBU(x,bits);
		}
	}
};

class InPacket
{
	vector<unsigned char> Data;
	int BitPos;
	bool Underflowed;
public:
	InPacket(const unsigned char *buffer,int size);
	InPacket(const OutPacket &op);
	int BytePos() {return (BitPos+7)/8;}
	bool GetBits(void *x,int NumBits);
	bool SkipBits(int NumBits);
	bool GetBool() {bool t;GetBits(&t,1);return t;}
	bool UnderFlow() const {return Underflowed;}

	int SizeBytes() const {return Data.size();}
	const unsigned char *GetData() const {return &Data[0];}
	bool GetBytes(void *x,int NumBytes) {return GetBits(x,NumBytes*8);}
	bool GetString(string &x);
	template <class T> bool Get(T &x) {return GetBytes(&x,sizeof(T));}
	template <class T> bool GetVRS(T &x)
	{
		int blocksize=sizeof(T)*2;
		unsigned char num=0;
		bool sign;
		x=0;
		if (GetBool())
		{
			sign=GetBool();
			GetBits(&num,2);
			assert(num<4);
			GetBits(&x,(num+1)*blocksize);
			if (sign)
				x=-x;
		}
		return Underflowed;
	}
	template <class T> bool GetVRU(T &x)
	{
		int blocksize=sizeof(T)*2;
		unsigned char num=0;
		x=0;
		if (GetBool())
		{
			GetBits(&num,2);
			assert(num<4);
			GetBits(&x,(num+1)*blocksize);
			assert(x>=0);
		}
		return Underflowed;
	}
	template <class T> bool Delta(T &x) 
	{
		if (GetBool())
		{
			Get(x);
			return true;
		}
		return false;
	}
	template <class T> bool DeltaVRS(T &x) 
	{
		if (GetBool())
		{
			GetVRS(x);
			return true;
		}
		return false;
	}
	template <class T> bool DeltaVRU(T &x) 
	{
		if (GetBool())
		{
			GetVRU(x);
			return true;
		}
		return false;
	}
	template <class T> bool GetBU(T &x,int bits) 
	{
		x=T(0);
		if (!bits)
			return Underflowed;
		GetBits(&x,bits);
		return Underflowed;
	}
	template <class T> bool DeltaBU(T &x,int bits) 
	{
		if (!bits)
		{
			x=T(0);
			return false;
		}
		if (GetBool())
		{
			GetBU(x,bits);
			return true;
		}
		return false;
	}
};

#endif

