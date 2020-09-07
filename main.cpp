﻿#include <stdio.h>
#include "ffCudaNn.h"

int mnist();
int cifar10();

int simple()
{
#if 1
	ff::CudaNn nn;
	nn.AddFc(1000, 4096);
	nn.AddFc(4096, 1024);
	nn.AddDropout(0.5);
	nn.AddRelu();
	nn.AddFc(1024, 1024);
	nn.AddRelu();
	nn.AddFc(1024, 10);
	nn.AddSumOfSquares();

	ff::CudaTensor x(1000, 256);
	ff::CudaTensor y(10, 256);
	x.SetRandom();
	y.SetRandom();
#else
	ff::CudaNn nn;
	nn.AddConv2d(3, 1, 8, 1, 1);		// 8 * 8 * 8
	nn.AddRelu();
	nn.AddConv2d(3, 8, 16, 1, 1);		// 8 * 8 * 16
	nn.AddRelu();
	nn.AddConv2d(3, 16, 16, 1, 1);		// 8 * 8 * 16
	nn.AddRelu();
	nn.AddMaxPool();					// 4 * 4 * 16
	nn.AddFc(256, 10);
	nn.AddSumOfSquares();

	ff::CudaTensor x(8, 8, 1, 256);
	ff::CudaTensor y(10, 256);
	x.SetRandom();
	y.SetRandom();
#endif

	float learningRate = 0.0001f;
	const ff::CudaTensor* yPred = nullptr;
	for (int i = 0; i < 10000; ++i)
	{
		for (int j = 0; j < 10; ++j)
		{
			nn.Forward(&x, true);
			nn.Backward(&y);
			nn.UpdateWs(learningRate);
		}

		yPred = nn.Forward(&x);
		const_cast<ff::CudaTensor*>(yPred)->PullFromGpu();

		float loss = 0.0;
		for (int r = 0; r < yPred->_d1; ++r)
		{
			for (int c = 0; c < yPred->_d0; ++c)
			{
				int index = c + r * yPred->_d0;
				float diff = yPred->_data[index] - y._data[index];
				loss += (diff * diff);
			}
		}
		printf("[%05d]loss: %f\n", i, loss / yPred->_d1);

	}
	return 0;
}

int main()
{
	/*
	ff::CudaTensor x;
	x.ResetTensor(3, 3);
	for (int i = 0; i < 9; ++i)
	{
		x._data[i] = i * 0.1f;
	}
	x.PushToGpu();
	ff::CudaTensor y;
	y.ResetTensor(1);
	y._data[0] = 1;
	y.PushToGpu();
	ff::CudaNn nn;
	nn.AddConv2d(2, 1, 1, 1, 1);
	nn.AddRelu();
	nn.AddMaxPool();
	nn.AddFc(4, 1);
	nn.Forward(&x);
	nn.Backward(&y);
	nn.Pull();
	*/


	return cifar10();
	//return mnist();
	//return simple();
}