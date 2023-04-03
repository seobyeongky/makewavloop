#include <iostream>
#include <stdio.h>
#include <math.h>
#include <vector>
#include <string>
#include <algorithm>

#include "AudioFile.h"

using namespace std;


void FadeIn(vector<double> & sample, int idx, int len);
void FadeOut(vector<double> & sample, int idx, int len);
void Commit(vector<double> & srcBuf, vector<double> & destBuf, int src, int dest, int len);
void PrintHelp();


int main(int argc, char* argv[])
{
	if (argc == 1)
	{
		printf("usage : makewavloop [filepath]\n");
		printf("        makewavloop [filepath] 0.1 0.2\n");
		printf("        makewavloop --help\n");
		return 1;
	}

	if (!strcmp(argv[1], "--help"))
	{
		PrintHelp();
		return 1;
	}

	AudioFile<double> audioFile;
	std::string path = argv[1];
	audioFile.load(path);

	int sampleRate = audioFile.getSampleRate();
	int bitDepth = audioFile.getBitDepth();

	int numSamples = audioFile.getNumSamplesPerChannel();
	double lengthInSeconds = audioFile.getLengthInSeconds();

	int numChannels = audioFile.getNumChannels();
	bool isMono = audioFile.isMono();
	bool isStereo = audioFile.isStereo();

	// or, just use this quick shortcut to print a summary to the console
	audioFile.printSummary();

	// 1. Create an AudioBuffer 
	// (BTW, AudioBuffer is just a vector of vectors)

	AudioFile<double>::AudioBuffer buffer;

	buffer.resize(numChannels);


	/*
	*   0       frontEnd    backStart   numSamples
	*   |  fadein  |  preserve  |   fadeout |


	*  => | fadeout x fadein | preserve |
	*             ↑
	*       fadeout이 preserve 뒤와 연결되기 때문에 앞쪽에 위치하고,
	*       fadein은 preserve 앞쪽과 연결되기 때문에 뒤쪽에 위치한다.
	*       둘은 더해지고, 길이는 둘 중 긴 것을 기준으로 잡힌다
	*/
	int frontEnd = numSamples / 2;
	int backStart = numSamples / 2;

	if (argc >= 4)
	{
		frontEnd = floor(stof(argv[2]) * numSamples);
		backStart = numSamples - floor(stof(argv[3]) * numSamples);
		if (backStart < frontEnd) // 값 처리
		{
			if (backStart < frontEnd - 1) // 유저가 잘못 입력한 것일 가능성이 높은 경우
				printf("값입력이잘못되었음 뒤비율값이 너무 크다 %d < %d", backStart, frontEnd);

			backStart = frontEnd;
		}
	}

	assert(frontEnd <= backStart);

	int mixLen = max(frontEnd, numSamples - backStart);
	int preserveLen = backStart - frontEnd;



	/*  ex)
		0 1 2 3 4 5 6 7 8
		a b c d e f g h i

		frontEnd : 2
		backStart : 5

		= > mixLen = 4
		= > preserve - len : 3

		0 1 2 3 4 5 6
		..a b c d e
		f g h i
     */

	// 3. Set number of samples per channel
	for (int i = 0; i < numChannels; i++)
	{
		//buffer[i].resize(numSamples - M);
		buffer[i].resize(mixLen + preserveLen);
	}

	// 4. do something here to fill the buffer with samples, e.g.
	for (int channel = 0; channel < numChannels; channel++)
	{
		auto & sample = audioFile.samples[channel];
		auto & buf = buffer[channel];

		// preserve part : simple copy
		Commit(sample, buf, frontEnd, mixLen, preserveLen);

		// back part : fade-out and copy
		FadeOut(sample, backStart, numSamples - backStart);
		Commit(sample, buf, backStart, 0, numSamples - backStart);

		// front part : fade-in and copy
		FadeIn(sample, 0, frontEnd);
		Commit(sample, buf, 0, mixLen - frontEnd, frontEnd);
	}

	// 5. rotate shifting (fadein part가 약간 밀려있을 수 있기 때문에)
	if (mixLen - frontEnd > 0)
	{
		for (int channel = 0; channel < numChannels; channel++)
		{
			auto & buf = buffer[channel];

			rotate(buf.begin(),
				buf.begin() + mixLen - frontEnd, // this will be the new first element
				buf.end());
		}
	}

	// 6. Put into the AudioFile object
	if (!audioFile.setAudioBuffer(buffer))
	{
		printf("?\n");
		return 1;
	}

	{
		size_t lastindex = path.find_last_of(".");
		string rawname = path.substr(0, lastindex);
		rawname += "_loop";
		string ext = path.substr(1 + lastindex);
		
		if (!audioFile.save(rawname + '.' + ext))
		{
			printf("Failed to save\n");
			return 1;
		}
	}


	return 0;
}

void FadeIn(vector<double> & sample, int idx, int len)
{
	int count = 0;
	for (int i = idx; i < idx + len; i++, count++)
	{
		double progress = (double)count / len;
		sample[i] *= progress;
	}
}

void FadeOut(vector<double> & sample, int idx, int len)
{
	int count = 0;
	for (int i = idx; i < idx + len; i++, count++)
	{
		double progress = (double)count / len;
		sample[i] *= (1 - progress);
	}
}

void Commit(vector<double> & srcBuf, vector<double> & destBuf, int src, int dest, int len)
{
	for (int i = 0; i < len; i++)
	{
		destBuf[i + dest] += srcBuf[i + src];
	}
}


void PrintHelp()
{
	printf("원리설명 : 기본적으로 사운드가 있으면 loop를 하면 소리가 튄다\n");
	printf("그건 시작과 끝이 불연속적이기 때문이다\n");
	printf("그래서 그냥 둘의 volume을 0으로 만들어버리는 프로그램인 것이다\n");
	printf("기본적으로 앞,뒤를 반띵해서 앞쪽은 fade in, 뒤쪽은 fade out시키고 더한다\n");
	printf("그래서 길이는 절반이 되고 seamless하게 들리게 된다");
	printf("만약 소리를 좀 보존시키고 싶다면 비율값을 입력하라");
	printf("예를 들면 0.1, 0.2로 입력한다면, 초반 10%%, 뒤 20%%는 섞이게 되지만 중간부분(70%%)은 보존된다는 것");
}
