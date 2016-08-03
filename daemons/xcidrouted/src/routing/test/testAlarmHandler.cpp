#include <cmath>
#include <iostream>
#include <chrono>
#include <thread>
#include <functional>

using namespace std;

double nextWaitTimeInSecond(double ratePerSecond){
	double currRand = (double)rand()/(double)RAND_MAX;
	double nextTime = -1*log(currRand)/ratePerSecond;	// next time in second
	return nextTime;	
}

void do_something(int nextTimeMilisecond)
{
    cout << "nextTimeMilisecond: " << nextTimeMilisecond << endl;
}

void timer_start()
{

    thread([](){
    	// sleep for 10 seconds for hello message to propagate
		this_thread::sleep_for(std::chrono::seconds(10));
        while (true)
        {
            double nextTimeInSecond = nextWaitTimeInSecond(1);
            int nextTimeMilisecond = (int) ceil(nextTimeInSecond * 1000);
            do_something(nextTimeMilisecond);
            this_thread::sleep_for(chrono::milliseconds(nextTimeMilisecond));
        }
    }).detach();
}


int main()
{
	timer_start();

	printf("I am here?\n");

	while(1){};
	return 0;
}