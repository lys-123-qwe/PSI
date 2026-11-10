
#include <fstream>
#include <algorithm>
#include <stdlib.h>
#include <math.h>
#include <NTL/GF2X.h>
#include <NTL/GF2.h>
#include <NTL/GF2XFactoring.h>
#include <NTL/vector.h>
#include <NTL/BasicThreadPool.h>
#include <omp.h>
#include<thread>
#include<vector>
#include<ctime>
#include<cmath>
#include <sstream>
#include <bitset>
#include <boost/dynamic_bitset.hpp>
#include <time.h>
using namespace boost;
using namespace std;
using namespace NTL;

string GF2X_to_String(GF2X x) {
	vector<char> arr;
	for (int i = 0; i < deg(x) + 1; i++)
	{
		if (x[i] == GF2(0))
		{
			arr.push_back('0');
		}
		else
		{
			arr.push_back('1');
		}
	}
	string ret;
	for (int i = 0; i < arr.size(); i++)
	{
		ret += arr[i];
	}
	return ret;
}

unsigned long GF2X_to_Decimal(GF2X x, int m) {
	vector<char> arr;
	for (int i = 0; i < deg(x) + 1; i++)
	{
		if (x[i] == GF2(0))
		{
			arr.push_back('0');
		}
		else
		{
			arr.push_back('1');
		}
	}
	string ret;
	for (int i = 0; i < arr.size(); i++)
	{
		ret += arr[i];
	}
	for (int i = 0; i < m - deg(x) - 1; i++)
	{
		ret += '0';
	}
	bitset<64> t(ret);
	return t.to_ulong();
}

vector<string> generateSequence(const int n) {

	vector<string> vec_Sequence;
	//bitset b = new bitset(n);


	for (int i = 0; i < pow(2, n); i++)
	{
		dynamic_bitset<> db(n, i);
		string s;
		to_string(db, s);
		//cout << bitset<n>(i) << endl;
		vec_Sequence.push_back(s);
	}
	return vec_Sequence;
}


Vec<GF2X> complement(GF2X x, int m, int k) {
	Vec<GF2X> complement_arr;
	//int n = m - 1 - deg(x);
	int n = m - k;
	vector<string> vec_Sequence = generateSequence(n);

	for (int i = 0; i < vec_Sequence.size(); i++)
	{
		GF2X a = x;
		for (int j = k, l = 0; j < m - 1, l < vec_Sequence[i].length(); j++, l++)
		{
			SetCoeff(a, j, vec_Sequence[i][l]);
		}
		complement_arr.append(a);
	}
	return complement_arr;
}

GF2X GS_Hash(GF2X a, GF2X b, GF2X x, int m, int k, GF2X f) {
	//GF2X f = GF2X();
	//f.SetLength(k);
	//f.normalize();
	//GF2X hash_result = (MulTrunc(a, x, m) + b);

	GF2X result = MulMod(a, x, f) + b;
	GF2X hash_result;

	for (int i = 0; i < k; i++)
	{
		SetCoeff(hash_result, i, result[i]);
	}
	//cout << "result" << result << endl;
	//cout << "hash_result>" << hash_result << endl;
	//cout << "deg(hash_result)" << deg(hash_result) << endl;
	return hash_result;
}
// op method
int GS_Protocol_sort_search(int i, vector<long>& correctSet, vector<long> Set, int N, int m, int k, GF2X a, GF2X b, GF2X y, GF2X f) {
	//
	int res = 0;
	// complement
	Vec<GF2X> complement_arr = complement(y, m, k);
	// binary_search
	for (int i = 0; i < complement_arr.length(); i++)
	{

		GF2X z = MulMod(InvMod(a, f), (complement_arr[i] - b), f);

		long dec_z = GF2X_to_Decimal(z, m);

		bool search_res = binary_search(Set.begin(), Set.end(), dec_z);

		if (search_res)
		{
			//cout << dec_z << endl;
			correctSet.push_back(dec_z);
			res = 1;
		}
		//cout << "--------------------------" << endl;
	}
	return res;
}

//native method
int GS_Protocol(int i, Vec<GF2X>& correctSet, Vec<GF2X> Set, int N, int m, int k, GF2X a, GF2X b, GF2X y, GF2X f) {
	std::cout << "-------Round<" << i << ">--------" << endl;
	int res = 0;
	//#pragma omp parallel for
	for (int i = 0; i < N; i++) {

		GF2X x = Set[i];
		GF2X hash_value = GS_Hash(a, b, x, m, k, f);
		if (hash_value == y)
		{
			//std::cout << i << endl;
			correctSet.append(x);
			res = 1;

		}
	}
	return res;
}
int compare(Vec<GF2X> setA, Vec<GF2X> setB) {
	for (int i = 0; i < setA.length(); i++)
	{
		for (int j = 0; j < setB.length(); j++)
		{
			if (setA[i] == setB[j])
			{
				return 1;
			}
		}
	}
	return 0;
}
int compare2(vector<long> setA, vector<long> setB) {
	for (int i = 0; i < setA.size(); i++)
	{
		for (int j = 0; j < setB.size(); j++)
		{
			if (setA[i] == setB[j])
			{
				return 1;
			}
		}
	}
	return 0;
}

int judge(Vec<GF2X> set, GF2X x) {

	if (set.position(x) == -1)
	{
		return 1;
	}
	return 0;
}

int judge2(Vec<GF2X> set, GF2X x) {

	for (int i = 0; i < set.length(); i++)
	{
		//cout <<"set:::" <<set[i] << endl;
		//cout << "x:::" << x << endl;
		//bool res = set[i] == x;
		//cout << "res:::" <<res  << endl;
		if (set[i] == x)
		{
			return 0;
		}
	}
	return 1;
}

template <class Type>
Type stringToNum(const string& str) {
	istringstream iss(str);
	Type num;
	iss >> num;
	return num;
}
int trans(vector<int> arr) {
	int result = 0;
	int bit = 1;
	for (int i = 0; i < 64; i++)
	{
		result = result << 1;
		if (arr[i] == 1) {
			result = result | bit;
		}
	}
	return result;
}

int test(int n1, vector<long> P1_data, vector<long> P2_data, int N, int m, int k, GF2X f) {
	int count = 0;
	int P1_count = 0;
	int P2_count = 0;

	NTL_EXEC_RANGE(n1, first, last)
		for (int i = first; i < last; i++)
		{
		GF2X a = random_GF2X(m);
		GF2X b = random_GF2X(m);
		GF2X y = random_GF2X(k);
		//GF2X a = vec_a[i];
		//GF2X b = vec_b[i];
		//GF2X y = vec_y[i];

		vector<long> P1_correct_set, P2_correct_set;
		P1_count += GS_Protocol_sort_search(i, P1_correct_set, P1_data, N, m, k, a, b, y, f);
		P2_count += GS_Protocol_sort_search(i, P2_correct_set, P2_data, N, m, k, a, b, y, f);


		if (compare2(P1_correct_set, P2_correct_set))
		{
			count += 1;
		}
	       }
	       	


		
		
	NTL_EXEC_RANGE_END
	
	return count;

}

int main() {
	//PRNG prng(_mm_set_epi32(4253465, 3434565, 234435, 23987045));
	//block ce = prng.get<block>();
	clock_t start = clock();
	auto n = thread::hardware_concurrency();
	cout << n << endl;
	int Round = 5000;
	int N = pow(2, 18);
	float p = (float)1 / 2;
	cout << "p=" << p << endl;
	float threshold = 0.5;
	int k = log2((int)(threshold * N)) + 1;
	int m = k + 8;
	cout << "k=:" << k << endl;
	cout << "m=:" << m << endl;
	string f_filename = "f20.txt";
	string P1_set_filename = "p1_20.txt";
	string P1_sort_filename = "p1_sort_20.txt";
	string P2_set_filename = "p2_20.txt";
	string P2_sort_filename = "p2_sort_20.txt";
	string a_filename = "a20.txt";
	string b_filename = "b20.txt";
	string y_filename = "y20.txt";

	//// write files
	GF2X f = BuildIrred_GF2X(m);

	cout << "deg(f):" << deg(f) << endl;

	// generate P1 sets
	Vec<GF2X> P1_Set;
	P1_Set.SetLength(N);
	for (int i = 0; i < N; i++) {

		GF2X x = random_GF2X(m);
		P1_Set.put(i, random_GF2X(m));

	}

	vector<long> P1_data;
	for (int i = 0; i < N; i++) {

		P1_data.push_back(GF2X_to_Decimal(P1_Set[i], m));
	}
	sort(P1_data.begin(), P1_data.end());


	// generate P2 sets
	Vec<GF2X> P2_Set;
	P2_Set.SetLength(N);
	float frac = 0.38;
	int sameNum = (int)(frac * N);
	//cout << "sameNum=" << sameNum << endl;

	for (int i = 0; i < sameNum; i++) {
		P2_Set[i] = P1_Set[i];
	}

	for (int i = sameNum; i < N; i++) {

		GF2X x = random_GF2X(m);
		P2_Set.put(i, random_GF2X(m));
	}

	vector<long> P2_data;
	for (int i = 0; i < N; i++) {

		P2_data.push_back(GF2X_to_Decimal(P2_Set[i], m));
	}
	sort(P2_data.begin(), P2_data.end());




	
	int P1_count = 0;
	int P2_count = 0;
	int flag = 0;
	long n1 = Round;
	int count = 0;
       float ex = (float)5 / 8;
	int target = (int)(ex * p * Round);
	for(int l = 0;l<100;l++){
	std::cout << "-------Round<" << l << ">--------" << endl;
	count = test(n1, P1_data, P2_data, N, m, k, f);
	
	if(count>=target){
	flag++;
	}
	
	}


	cout << "count is =" << flag << endl;
	clock_t end = clock();
	double endtime = (double)(end - start) / CLOCKS_PER_SEC;
	cout << "Total time===" << endtime << endl;		//sec




	return 0;
}
