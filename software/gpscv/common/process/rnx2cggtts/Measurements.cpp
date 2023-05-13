//
//
// The MIT License (MIT)
//
// Copyright (c) 2023  Michael J. Wouters
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

#include <cmath>
#include <iostream>

#include "Measurements.h"

//
//	Public
//

Measurements::Measurements()
{
	gnss = 0;
	maxMeas = 0;
	maxSVN = 0;
	meas = NULL;
}

Measurements::~Measurements()
{
	if (NULL != meas){
		for (int i = 0; i < maxMeas; i++) {
			for (int j=0; j <= maxSVN; j++){
				delete [] meas[i][j];
			}
			delete[] meas[i];
		}	
		delete [] meas;
	}
}

void Measurements::dump(){
	std::cout << "GNSS: " << gnss << std::endl;
	std::cout.precision(14);
	for (int i = 0; i < 10; i++) {
		
		for (int j=1; j <= maxSVN; j++){
			std::cout << j << " ";
			for (int k=0; k< nCodeObs+2; k++){
				std::cout << meas[i][j][k] << " ";
			}
			std::cout << std::endl;
		}
	}
}//

void Measurements::allocateStorage(int nMeasEpochs)
{
	maxMeas = nMeasEpochs;
	int n = codes.size() + 2; // extra fields are for MJD and TOD in seconds 
	meas = new double**[nMeasEpochs];
	for (int i = 0; i < nMeasEpochs ; i++) { 
    meas[i] = new double*[maxSVN + 1]; // dummy at beginning so indexing is directly by SVN
		for (int j=0;j<= maxSVN;j++){
			meas[i][j] = new double[n]; 
			for (int k=0;k<n;k++)
				meas[i][j][k] = NAN; 
		}
	}
	
}

// Return column that the code is in
// Code names are stored as given in the obersvation files
int Measurements::colIndexFromCode(std::string c){
	for (unsigned int i=0;i<codes.size();i++){
		if (codes[i] == c){
			return i;
		}
	}
	return -1; // FIXME probably should sanity check RINEX file before we get this far
}

//
// Private methods
//


