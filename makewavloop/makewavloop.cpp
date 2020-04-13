#include <iostream>
#include <stdio.h>
#include <math.h>
#include <vector>
#include <string>

#include "AudioFile.h"

using namespace std;


void FadeIn(vector<double> & sample, int idx, int len);
void FadeOut(vector<double> & sample, int idx, int len);
void Commit(vector<double> & srcBuf, vector<double> & destBuf, int src, int dest, int len);


int main(int argc, char* argv[])
{
	if (argc == 1)
	{
		printf("usage : makewavloop [filepath]\n");
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

	int M = numSamples / 2;
	assert(2 * M <= numSamples);

	// 3. Set number of samples per channel
	for (int i = 0; i < numChannels; i++)
	{
		buffer[i].resize(numSamples - M);
	}

	// 4. do something here to fill the buffer with samples, e.g.
	for (int channel = 0; channel < numChannels; channel++)
	{
		auto & sample = audioFile.samples[channel];
		auto & buf = buffer[channel];

		// middle part : simple copy
		Commit(sample, buf, M, 0, numSamples - 2 * M);

		// back part : fade-out and copy
		FadeOut(sample, numSamples - M, M);
		Commit(sample, buf, numSamples - M, numSamples - 2 * M, M);

		// front part : fade-in and copy
		FadeIn(sample, 0, M);
		Commit(sample, buf, 0, numSamples - 2 * M, M);
	}

	// 5. Put into the AudioFile object
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


