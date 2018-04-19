#ifndef __ANN_H
#define __ANN_H

// -----------------------------------------------------------------------------
int ground_truth(					// find ground truth
	int   n,							// number of data objects
	int   qn,							// number of query objects
	int   d,							// dimensionality
	float p,							// the p value of Lp norm, p in (0,2]
	const char *data_set,				// address of data  set
	const char *query_set,				// address of query set
	const char *truth_set);				// address of truth set

// -----------------------------------------------------------------------------
int indexing(						// indexing of qalsh
	int   n,							// number of data objects
	int   d,							// dimensionality
	int   B,							// page size
	float p,							// the p value of Lp norm, p in (0,2]
	float zeta,							// symmetric factor of p-stable distr.
	float ratio,						// approximation ratio
	const char *data_set,				// address of data set
	const char *data_folder,			// data folder
	const char *output_folder);			// output folder

// -----------------------------------------------------------------------------
int lshknn(							// k-NN search of qalsh
	int   qn,							// number of query objects
	int   d,							// dimensionality
	const char *query_set,				// address of query set
	const char *truth_set,				// address of truth set
	const char *data_folder,			// data folder
	const char *output_folder);			// output folder

// -----------------------------------------------------------------------------
int linear_scan(					// brute-force linear scan (data in disk)
	int   n,							// number of data objects
	int   qn,							// number of query objects
	int   d,							// dimensionality
	int   B,							// page size
	float p,							// the p value of Lp norm, p in (0,2]
	const char *query_set,				// address of query set
	const char *truth_set,				// address of truth set
	const char *data_folder,			// data folder
	const char *output_folder);			// output folder

#endif // __ANN_H
