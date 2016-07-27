#include <random>
#include <algorithm>
#include <iostream>
#include <sstream>
#include <fstream>
#include <cmath>

using namespace std;

int main(int argc, char const *argv[])
{
	ofstream currFile;
	currFile.open("plot.dat");

	srand((unsigned)time(NULL));

	for(int i  = 0; i < 10000; i++){
		double currRand = (double)rand()/(double)RAND_MAX;
		double nextTime = -1*log(currRand)/10.0;

		currFile << currRand << " " << nextTime << endl;
	}

	currFile.close();
	return 0;
}