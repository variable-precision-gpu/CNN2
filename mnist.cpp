﻿#include <stdio.h>
#include <assert.h>
#include <vector>
#include <algorithm>
#include <math.h>
#include "ffCudaNn.h"

#if !defined(__FF_WINDOWS__)
#define _byteswap_ulong(x)	__bswap_32((x))
#endif

void LoadMnistData(const char* imageFile, const char* labelFile, const int batchSize, std::vector<ff::CudaTensor>& images, std::vector<ff::CudaTensor>& labels)
{
	// Image
	FILE* fp = fopen(imageFile, "rb");
	assert(nullptr != fp);
	int magic = 0, nImages = 0, nRow = 0, nCol = 0;
	fread(&magic, sizeof(magic), 1, fp);
	fread(&nImages, sizeof(nImages), 1, fp);
	fread(&nRow, sizeof(nRow), 1, fp);
	fread(&nCol, sizeof(nCol), 1, fp);
	nImages = _byteswap_ulong(nImages);
	nRow = _byteswap_ulong(nRow);
	nCol = _byteswap_ulong(nCol);
	assert(28 == nRow && 28 == nCol);
	int nPixels = nRow * nCol;
	unsigned char* imageRaw = new unsigned char[nPixels * nImages];
	fread(imageRaw, nPixels * sizeof(unsigned char), nImages, fp);
	fclose(fp);

	int nImages2 = nImages;
	int numBatches = (nImages + batchSize - 1) / batchSize;
	images.resize(numBatches);
	for (int i = 0; i < numBatches; ++i)
	{
		int currBatchSize = (batchSize < nImages2 ? batchSize : nImages2);
		images[i].ResetTensor(nPixels, currBatchSize);
		for (int j = 0; j < currBatchSize; ++j)
		{
			for (int k = 0; k < nPixels; ++k)
			{
				images[i]._data[k + j * nPixels] = imageRaw[(i * batchSize + j) * nPixels + k] / 255.0f;
			}
		}
		images[i].PushToGpu();
		nImages2 -= batchSize;
	}
	delete[] imageRaw;

	// Label
	fp = fopen(labelFile, "rb");
	assert(nullptr != fp);
	int nLabels = 0;
	fread(&magic, sizeof(magic), 1, fp);
	fread(&nLabels, sizeof(nLabels), 1, fp);
	nLabels = _byteswap_ulong(nLabels);
	assert(nLabels == nImages);
	unsigned char* labelRaw = new unsigned char[nLabels];
	fread(labelRaw, sizeof(unsigned char), nLabels, fp);
	fclose(fp);

	labels.resize(numBatches);
	for (int i = 0; i < numBatches; ++i)
	{
		int currBatchSize = (batchSize < nLabels ? batchSize : nLabels);
		labels[i].ResetTensor(currBatchSize);
		for (int j = 0; j < currBatchSize; ++j)
		{
			labels[i]._data[j] = labelRaw[i * batchSize + j];
		}
		labels[i].PushToGpu();
		nLabels -= batchSize;
	}
	delete[] labelRaw;
}

void CheckAccuracy(const ff::CudaTensor* pSoftmax, const ff::CudaTensor& yLabel, int& top1, int& top3, int& top5)
{
	struct Element
	{
		int		_index;
		float	_softmax;
	} e;

	int result[3] = { 0, 0, 0 };
	std::vector<Element> arr;
	for (int r = 0; r < pSoftmax->_d1; ++r)
	{
		arr.clear();
		int yIndex = static_cast <int> (yLabel._data[r]);
		for (int c = 0; c < pSoftmax->_d0; ++c)
		{
			e._index = c;
			e._softmax = pSoftmax->_data[c + r * pSoftmax->_d0];
			arr.push_back(e);
		}

		std::sort(arr.begin(), arr.end(), [](const Element& lhs, const Element& rhs) {
			return lhs._softmax > rhs._softmax; });

		const int cut[3] = { 1, 3, 5 };
		for (int i = 0; i < 3; ++i)
		{
			for (int j = 0; j < cut[i]; ++j)
				if (arr[j]._index == yIndex)
				{
					++result[i];
					break;
				}
		}
	}
	top1 = result[0];
	top3 = result[1];
	top5 = result[2];
}

int mnist()
{
	const int kBatchSize = 100;

	std::vector<ff::CudaTensor> trainingImages;
	std::vector<ff::CudaTensor> trainingLabels;
	std::vector<ff::CudaTensor> testImages;
	std::vector<ff::CudaTensor> testLabels;
	LoadMnistData("mnist/train-images.idx3-ubyte", "mnist/train-labels.idx1-ubyte", kBatchSize, trainingImages, trainingLabels);
	LoadMnistData("mnist/t10k-images.idx3-ubyte", "mnist/t10k-labels.idx1-ubyte", 10000, testImages, testLabels);

	ff::CudaNn nn;
	nn.InitializeCudaNn("");
	nn.AddFc(28 * 28, 1000);
	nn.AddDropout(0.5);
	nn.AddReluFc(1000, 1000);
	nn.AddDropout(0.5);
	nn.AddReluFc(1000, 10);
	nn.AddSoftmax();

	const int numEpoch = 1000;
	const size_t numBatch = trainingImages.size();

	float learningRate = 0.001f;
	float lowest_loss = 1e8f;
	for (int i = 0; i < numEpoch; ++i)
	{
		for (size_t j = 0; j < numBatch; ++j)
		{
			nn.Forward(&trainingImages[j], true);
			nn.Backward(&trainingLabels[j]);
			nn.UpdateWs(learningRate);
		}

		ff::CudaTensor* softmax = const_cast<ff::CudaTensor*>(nn.Forward(&testImages[0]));
		softmax->PullFromGpu();

		float loss = 0.0;
		for (int j = 0; j < testImages[0]._d1; ++j)
		{
			loss += -logf(softmax->_data[static_cast<int>(testLabels[0]._data[j]) + softmax->_d0 * j]);
		}
		loss /= testImages[0]._d1;

		if (loss < lowest_loss)
		{
			lowest_loss = loss;
		}
		else
		{
			// Learning rate decay
			learningRate *= 0.4f;
		}

		int top1 = 0, top3 = 0, top5 = 0;
		CheckAccuracy(softmax, testLabels[0], top1, top3, top5);
		printf("Epoch[%03d] Test[%d](Loss: %f/%f, Top1: %d, Top3: %d, Top5: %d)\n", i+1, softmax->_d1, loss, lowest_loss,
			top1,
			top3,
			top5);
	}
	return 0;
}
