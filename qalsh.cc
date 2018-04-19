#include "headers.h"

// -----------------------------------------------------------------------------
QALSH::QALSH()						// constructor
{
	n_pts_       = -1;
	dim_         = -1;
	B_           = -1;
	p_           = -1.0f;
	zeta_        = -1.0f;
	appr_ratio_  = -1.0f;
	
	w_           = -1.0f;
	p1_          = -1.0f;
	p2_          = -1.0f;
	alpha_       = -1.0f;
	beta_        = -1.0f;
	delta_       = -1.0f;
	m_           = -1;
	l_           = -1;
	a_array_     = NULL;
	trees_       = NULL;

	dist_io_     = -1;
	page_io_     = -1;
	freq_        = NULL;
	checked_     = NULL;
	flag_        = NULL;
	data_        = NULL;
	q_val_       = NULL;
	lptr_        = NULL;
	rptr_        = NULL;
}

// -----------------------------------------------------------------------------
QALSH::~QALSH()						// destructor
{
	if (a_array_ != NULL) {
		delete[] a_array_; a_array_ = NULL;
	}
	if (trees_ != NULL) {
		for (int i = 0; i < m_; ++i) {
			delete trees_[i]; trees_[i] = NULL;
		}
		delete[] trees_; trees_ = NULL;
	}

	if (freq_ != NULL) {
		delete[] freq_; freq_ = NULL;
	}
	if (checked_ != NULL) {
		delete[] checked_; checked_ = NULL;
	}
	if (flag_ != NULL) {
		delete[] flag_; flag_ = NULL;
	}
	if (data_ != NULL) {
		delete[] data_; data_ = NULL;
	}
	if (q_val_ != NULL) {
		delete[] q_val_; q_val_ = NULL;
	}
	if (lptr_ != NULL) {
		delete[] lptr_; lptr_ = NULL;
	}
	if (rptr_ != NULL) {
		delete[] rptr_; rptr_ = NULL;
	}
}

// -----------------------------------------------------------------------------
int QALSH::build(					// build index
	int   n,							// number of data points
	int   d,							// dimension of space
	int   B,							// page size
	float p,							// the p value of L_p norm
	float zeta,							// symmetric factor of p-stable distr.
	float ratio,						// approximation ratio
	const float **data,					// data objects
	const char *index_path)				// index path
{
	// -------------------------------------------------------------------------
	//  init parameters
	// -------------------------------------------------------------------------
	n_pts_      = n;
	dim_        = d;
	B_          = B;
	p_          = p;
	zeta_       = zeta;
	appr_ratio_ = ratio;

	strcpy(index_path_, index_path);
	create_dir(index_path_);

	// -------------------------------------------------------------------------
	//  calc parameters and generate hash functions
	// -------------------------------------------------------------------------
	calc_params();
	gen_hash_func();
	display();

	// -------------------------------------------------------------------------
	//  bulkloading
	// -------------------------------------------------------------------------
	bulkload(data);
}

// -----------------------------------------------------------------------------
void QALSH::calc_params()			// calc params of qalsh
{
	delta_ = 1.0f / E;
	beta_  = (float) CANDIDATES / (float) n_pts_;

	// -------------------------------------------------------------------------
	//  init <w_> <p1_> and <p2_> (auto tuning-w)
	//  
	//  w0 ----- best w for L_{0.5} norm to minimize m (auto tuning-w)
	//  w1 ----- best w for L_{1.0} norm to minimize m (auto tuning-w)
	//  w2 ----- best w for L_{2.0} norm to minimize m (auto tuning-w)
	//  other w: use linear combination for interpolation
	// -------------------------------------------------------------------------
	float w0 = (appr_ratio_ - 1.0f) / log(sqrt(appr_ratio_));
	float w1 = 2.0f * sqrt(appr_ratio_);
	float w2 = sqrt((8.0f * appr_ratio_ * appr_ratio_ * log(appr_ratio_))
		/ (appr_ratio_ * appr_ratio_ - 1.0f));

	if (fabs(p_ - 0.5f) < FLOATZERO) {
		w_ = w0;
		p1_ = calc_l0_prob(w_ / 2.0f);
		p2_ = calc_l0_prob(w_ / (2.0f * appr_ratio_));
	}
	else if (fabs(p_ - 1.0f) < FLOATZERO) {
		w_ = w1;
		p1_ = calc_l1_prob(w_ / 2.0f);
		p2_ = calc_l1_prob(w_ / (2.0f * appr_ratio_));
	}
	else if (fabs(p_ - 2.0f) < FLOATZERO) {
		w_ = w2;
		p1_ = calc_l2_prob(w_ / 2.0f);
		p2_ = calc_l2_prob(w_ / (2.0f * appr_ratio_));
	}
	else {
		if (fabs(p_ - 0.8f) < FLOATZERO) {
			w_ = 2.503f;
		}
		else if (fabs(p_ - 1.2f) < FLOATZERO) {
			w_ = 3.151f;
		}
		else if (fabs(p_ - 1.5f) < FLOATZERO) {
			w_ = 3.465f;
		}
		else {
			w_ = (w2 - w1) * p_ + (2.0f * w1 - w2);
		}
		new_stable_prob(p_, zeta_, appr_ratio_, 1.0f, w_, 1000000, p1_, p2_);
	}

	float para1 = sqrt(log(2.0f / beta_));
	float para2 = sqrt(log(1.0f / delta_));
	float para3 = 2.0f * (p1_ - p2_) * (p1_ - p2_);

	float eta = para1 / para2;
	alpha_ = (eta * p1_ + p2_) / (1.0f + eta);

	m_ = (int) ceil((para1 + para2) * (para1 + para2) / para3);
	l_ = (int) ceil(alpha_ * m_);

	freq_    = new int[n_pts_];
	checked_ = new bool[n_pts_];
	flag_    = new bool[m_];
	data_    = new float[dim_];
	q_val_   = new float[m_];
	lptr_    = new PageBuffer[m_];
	rptr_    = new PageBuffer[m_];
}

// -----------------------------------------------------------------------------
float QALSH::calc_l0_prob(			// calc prob <p1_> and <p2_> of L1/2 dist
	float x)							// x = w / (2.0 * r)
{
	return new_levy_prob(x);
}

// -----------------------------------------------------------------------------
float QALSH::calc_l1_prob(			// calc prob <p1_> and <p2_> of L1 dist
	float x)							// x = w / (2.0 * r)
{
	return new_cauchy_prob(x);
}

// -----------------------------------------------------------------------------
float QALSH::calc_l2_prob(			// calc prob <p1_> and <p2_> of L2 dist
	float x)							// x = w / (2.0 * r)
{
	return new_gaussian_prob(x);
}

// -----------------------------------------------------------------------------
void QALSH::display()				// display parameters
{
	printf("Parameters of QALSH (L_%.1f Distance):\n", p_);
	printf("    n     = %d\n", n_pts_);
	printf("    d     = %d\n", dim_);
	printf("    B     = %d\n", B_);
	printf("    ratio = %f\n", appr_ratio_);
	printf("    w     = %f\n", w_);
	printf("    p1    = %f\n", p1_);
	printf("    p2    = %f\n", p2_);
	printf("    alpha = %f\n", alpha_);
	printf("    beta  = %f\n", beta_);
	printf("    delta = %f\n", delta_);
	printf("    zeta  = %f\n", zeta_);
	printf("    m     = %d\n", m_);
	printf("    l     = %d\n", l_);
	printf("    path  = %s\n", index_path_);
	printf("\n");
}

// -----------------------------------------------------------------------------
void QALSH::gen_hash_func()			// generate hash function <a_array>
{
	int size = m_ * dim_;
	a_array_ = new float[size];
	for (int i = 0; i < size; ++i) {
		if (fabs(p_ - 0.5f) < FLOATZERO) {
			a_array_[i] = levy(1.0f, 0.0f);
		}
		else if (fabs(p_ - 1.0f) < FLOATZERO) {
			a_array_[i] = cauchy(1.0f, 0.0f);
		}
		else if (fabs(p_ - 2.0f) < FLOATZERO) {
			a_array_[i] = gaussian(0.0f, 1.0f);
		}
		else {
			a_array_[i] = p_stable(p_, zeta_, 1.0f, 0.0f);
		}
	}
}

// -----------------------------------------------------------------------------
int QALSH::bulkload(				// build m b-trees by bulkloading
	const float **data)					// data set
{
	// -------------------------------------------------------------------------
	//  write parameters to disk
	// -------------------------------------------------------------------------
	if (write_params()) return 1;

	// -------------------------------------------------------------------------
	//  write hash tables (indexed by B+ Tree) to disk
	// -------------------------------------------------------------------------
	char fname[200];
	Result *hashtable = new Result[n_pts_];
	for (int i = 0; i < m_; ++i) {
		for (int j = 0; j < n_pts_; ++j) {
			hashtable[j].id_  = j;
			hashtable[j].key_ = calc_hash_value(i, data[j]);
		}

		qsort(hashtable, n_pts_, sizeof(Result), ResultComp);
		
		BTree *bt = new BTree();
		get_tree_filename(i, fname);
		bt->init(B_, fname);
		if (bt->bulkload(n_pts_, (const Result *) hashtable)) return 1;

		delete bt; bt = NULL;
	}

	// -------------------------------------------------------------------------
	//  release space
	// -------------------------------------------------------------------------
	delete[] hashtable; hashtable = NULL;

	return 0;
}

// -----------------------------------------------------------------------------
int QALSH::write_params()			// write parameters to disk
{
	char fname[200];
	strcpy(fname, index_path_);
	strcat(fname, "para");

	FILE *fp = fopen(fname, "r");
	if (fp)	{
		printf("Hash tables exist.\n");
		exit(1);
	}

	fp = fopen(fname, "w");
	if (!fp) {
		printf("I could not create %s.\n", fname);
		printf("Perhaps no such folder %s?\n", index_path_);
		return 1;
	}

	fprintf(fp, "n = %d\n", n_pts_);
	fprintf(fp, "d = %d\n", dim_);
	fprintf(fp, "B = %d\n", B_);

	fprintf(fp, "ratio = %f\n", appr_ratio_);
	fprintf(fp, "w = %f\n", w_);
	fprintf(fp, "p1 = %f\n", p1_);
	fprintf(fp, "p2 = %f\n", p2_);

	fprintf(fp, "p = %f\n", p_);
	fprintf(fp, "alpha = %f\n", alpha_);
	fprintf(fp, "beta = %f\n", beta_);
	fprintf(fp, "delta = %f\n", delta_);
	fprintf(fp, "zeta = %f\n", zeta_);

	fprintf(fp, "m = %d\n", m_);
	fprintf(fp, "l = %d\n", l_);

	int count = 0;
	for (int i = 0; i < m_; ++i) {
		fprintf(fp, "%f", a_array_[count++]);
		for (int j = 1; j < dim_; ++j) {
			fprintf(fp, " %f", a_array_[count++]);
		}
		fprintf(fp, "\n");
	}
	fclose(fp);	

	return 0;
}

// -----------------------------------------------------------------------------
float QALSH::calc_hash_value(		// calc hash value
	int   table_id,						// hash table id
	const float *point)					// input data object
{
	float ret = 0.0f;
	for (int i = 0; i < dim_; ++i) {
		ret += (a_array_[table_id * dim_ + i] * point[i]);
	}
	return ret;
}

// -----------------------------------------------------------------------------
void QALSH::get_tree_filename(		// get file name of b-tree
	int  tree_id,						// tree id, from 0 to m-1
	char *fname)						// file name (return)
{
	sprintf(fname, "%s%d.qalsh", index_path_, tree_id);
}

// -----------------------------------------------------------------------------
int QALSH::load(					// load index
	const char *index_path)				// index path
{
	// -------------------------------------------------------------------------
	//  init parameters
	// -------------------------------------------------------------------------
	strcpy(index_path_, index_path);

	// -------------------------------------------------------------------------
	//  read parameters from disk
	// -------------------------------------------------------------------------
	if (read_params()) return 1;

	// -------------------------------------------------------------------------
	//  read hash tables from disk
	// -------------------------------------------------------------------------
	char fname[200];
	trees_ = new BTree*[m_];
	for (int i = 0; i < m_; ++i) {
		get_tree_filename(i, fname);

		trees_[i] = new BTree();
		trees_[i]->init_restore(fname);
	}

	return 0;
}

// -----------------------------------------------------------------------------
int QALSH::read_params()			// read parameters from disk
{
	char fname[200];
	strcpy(fname, index_path_);
	strcat(fname, "para");

	FILE *fp = fopen(fname, "r");
	if (!fp) {
		printf("Could not open %s.\n", fname);
		return 1;
	}

	fscanf(fp, "n = %d\n", &n_pts_);
	fscanf(fp, "d = %d\n", &dim_);
	fscanf(fp, "B = %d\n", &B_);

	fscanf(fp, "ratio = %f\n", &appr_ratio_);
	fscanf(fp, "w = %f\n", &w_);
	fscanf(fp, "p1 = %f\n", &p1_);
	fscanf(fp, "p2 = %f\n", &p2_);

	fscanf(fp, "p = %f\n", &p_);
	fscanf(fp, "alpha = %f\n", &alpha_);
	fscanf(fp, "beta = %f\n", &beta_);
	fscanf(fp, "delta = %f\n", &delta_);
	fscanf(fp, "zeta = %f\n", &zeta_);

	fscanf(fp, "m = %d\n", &m_);
	fscanf(fp, "l = %d\n", &l_);
	
	a_array_ = new float[m_ * dim_];
	int count = 0;
	for (int i = 0; i < m_; ++i) {
		for (int j = 0; j < dim_; ++j) {
			fscanf(fp, "%f", &a_array_[count++]);
		}
		fscanf(fp, "\n");
	}
	fclose(fp);

	freq_    = new int[n_pts_];
	checked_ = new bool[n_pts_];
	flag_    = new bool[m_];
	data_    = new float[dim_];
	q_val_   = new float[m_];
	lptr_    = new PageBuffer[m_];
	rptr_    = new PageBuffer[m_];

	return 0;
}

// -----------------------------------------------------------------------------
int QALSH::knn(						// k-NN search
	int top_k,							// top-k value
	const float *query,					// query object
	const char *data_folder,			// data folder
	MinK_List *list)					// k-NN results (return)
{
	// -------------------------------------------------------------------------
	//  initialize parameters for k-NN search
	// -------------------------------------------------------------------------
	init_search_params(query);

	// -------------------------------------------------------------------------
	//  k-NN search
	// -------------------------------------------------------------------------
	int candidates = CANDIDATES + top_k - 1; // threshold of candidates
	int num_flag   = 0;				// used for bucket bound
	int id         = -1;			// data object id
	int count      = -1;			// count size in one page
	int start      = -1;			// start position
	int end        = -1;			// end position

	float dist     = -1.0f;			// real distance between data and query
	float ldist    = -1.0f;			// left  projected dist with query
	float rdist    = -1.0f;			// right projected dist with query
	float knn_dist = MAXREAL;		// kth NN dist

	float radius   = find_radius(q_val_, lptr_, rptr_);
	float bucket   = w_ * radius / 2.0f;

	while (true) {
		// ---------------------------------------------------------------------
		//  step 1: initialize the stop condition for current round
		// ---------------------------------------------------------------------
		num_flag = 0;
		for (int i = 0; i < m_; ++i) flag_[i] = true;

		// ---------------------------------------------------------------------
		//  step 2: find frequent objects
		// ---------------------------------------------------------------------
		while (num_flag < m_) {
			for (int i = 0; i < m_; ++i) {
				if (!flag_[i]) continue;

				// -------------------------------------------------------------
				//  step 2.1: compute <ldist> and <rdist>
				// -------------------------------------------------------------
				ldist = MAXREAL;
				if (lptr_[i].size_ != -1) ldist = calc_dist(q_val_[i], &lptr_[i]);

				rdist = MAXREAL;
				if (rptr_[i].size_ != -1) rdist = calc_dist(q_val_[i], &rptr_[i]);

				// -------------------------------------------------------------
				//  step 2.2: determine the closer direction (left or right)
				//  and do collision counting to find frequent objects.
				//
				//  for the frequent object, we calc the L_{p} distance with
				//  query, and update the k-nn result.
				// -------------------------------------------------------------
				if (ldist < bucket && ldist <= rdist) {
					count = lptr_[i].size_;
					end   = lptr_[i].leaf_pos_;
					start = end - count;
					for (int j = end; j > start; --j) {
						id = lptr_[i].leaf_node_->get_entry_id(j);
						if (checked_[id]) continue;

						if (++freq_[id] > l_) {
							checked_[id] = true;
							read_data_new_format(id, dim_, B_, data_folder, data_);

							dist = calc_lp_dist(dim_, p_, data_, query);
							knn_dist = list->insert(dist, id);

							if (++dist_io_ >= candidates) break;
						}
					}
					update_left_buffer(&rptr_[i], &lptr_[i]);
				}
				else if (rdist < bucket && ldist > rdist) {
					count = rptr_[i].size_;
					start = rptr_[i].leaf_pos_;
					end   = start + count;
					for (int j = start; j < end; ++j) {
						id = rptr_[i].leaf_node_->get_entry_id(j);
						if (checked_[id]) continue;

						if (++freq_[id] > l_) {
							checked_[id] = true;
							read_data_new_format(id, dim_, B_, data_folder, data_);

							dist = calc_lp_dist(dim_, p_, data_, query);
							knn_dist = list->insert(dist, id);

							if (++dist_io_ >= candidates) break;
						}
					}
					update_right_buffer(&lptr_[i], &rptr_[i]);
				}
				else {
					flag_[i] = false;
					num_flag++;
				}
				if (num_flag >= m_ || dist_io_ >= candidates) break;
			}
			if (num_flag >= m_ || dist_io_ >= candidates) break;
		}
		// ---------------------------------------------------------------------
		//  stop condition 1 & 2
		// ---------------------------------------------------------------------
		if (knn_dist < appr_ratio_ * radius && dist_io_ >= top_k) break;
		if (dist_io_ >= candidates) break;

		// ---------------------------------------------------------------------
		//  step 3: auto-update <radius>
		// ---------------------------------------------------------------------
		radius = update_radius(radius, q_val_, lptr_, rptr_);
		bucket = radius * w_ / 2.0f;
	}
	delete_tree_ptr();	

	return page_io_ + dist_io_;
}

// -----------------------------------------------------------------------------
void QALSH::init_search_params(		// init parameters for k-NN search
	const float *query)					// query object
{
	page_io_ = 0;
	dist_io_ = 0;

	for (int i = 0; i < n_pts_; ++i) {
		freq_[i]    = 0;
		checked_[i] = false;
	}

	for (int i = 0; i < m_; ++i) {
		lptr_[i].leaf_node_ = NULL;
		lptr_[i].index_pos_ = -1;
		lptr_[i].leaf_pos_  = -1;
		lptr_[i].size_      = -1;

		rptr_[i].leaf_node_ = NULL;
		rptr_[i].index_pos_ = -1;
		rptr_[i].leaf_pos_ = -1;
		rptr_[i].size_ = -1;
	}

	int  block      = -1;			// variables for index node
	int  follow     = -1;
	bool lescape    = false;
	int  pos         = -1;			// variables for leaf node
	int  increment   = -1;
	int  num_entries = -1;

	BIndexNode *index_node = NULL;

	for (int i = 0; i < m_; ++i) {
		q_val_[i] = calc_hash_value(i, query);
		block = trees_[i]->root_;

		if (block > 1) {
			// ---------------------------------------------------------------------
			//  at least two levels in the B+ Tree: index node and lead node
			// ---------------------------------------------------------------------
			index_node = new BIndexNode();
			index_node->init_restore(trees_[i], block);
			page_io_++;

			// ---------------------------------------------------------------------
			//  find the leaf node whose value is closest and larger than the key
			//  of query q
			// ---------------------------------------------------------------------
			lescape = false;		// locate the position of branch
			while (index_node->get_level() > 1) {
				follow = index_node->find_position_by_key(q_val_[i]);

				if (follow == -1) {	// if in the most left branch
					if (lescape) {	// scan the most left branch
						follow = 0;
					}
					else {
						if (block != trees_[i]->root_) {
							printf("No branch found\n");
							exit(1);
						}
						else {
							follow = 0;
							lescape = true;
						}
					}
				}
				block = index_node->get_son(follow);
				delete index_node; index_node = NULL;

				index_node = new BIndexNode();
				index_node->init_restore(trees_[i], block);
				page_io_++;			// access a new node (a new page)
			}

			// ---------------------------------------------------------------------
			//  after finding the leaf node whose value is closest to the key of
			//  query, initialize <lptrs[i]> and <rptrs[i]>.
			//
			//  <lescape> = true is that the query has no <lptrs>, the query is 
			//  the smallest value.
			// ---------------------------------------------------------------------
			follow = index_node->find_position_by_key(q_val_[i]);
			if (follow < 0) {
				lescape = true;
				follow = 0;
			}

			if (lescape) {			
				// -----------------------------------------------------------------
				//  only init right buffer
				// -----------------------------------------------------------------
				block = index_node->get_son(0);
				rptr_[i].leaf_node_ = new BLeafNode();
				rptr_[i].leaf_node_->init_restore(trees_[i], block);
				rptr_[i].index_pos_ = 0;
				rptr_[i].leaf_pos_ = 0;

				increment = rptr_[i].leaf_node_->get_increment();
				num_entries = rptr_[i].leaf_node_->get_num_entries();
				if (increment > num_entries) {
					rptr_[i].size_ = num_entries;
				}
				else {
					rptr_[i].size_ = increment;
				}
				page_io_++;
			}
			else {					
				// -----------------------------------------------------------------
				//  init left buffer
				// -----------------------------------------------------------------
				block = index_node->get_son(follow);
				lptr_[i].leaf_node_ = new BLeafNode();
				lptr_[i].leaf_node_->init_restore(trees_[i], block);

				pos = lptr_[i].leaf_node_->find_position_by_key(q_val_[i]);
				if (pos < 0) pos = 0;
				lptr_[i].index_pos_ = pos;

				increment = lptr_[i].leaf_node_->get_increment();
				if (pos == lptr_[i].leaf_node_->get_num_keys() - 1) {
					num_entries = lptr_[i].leaf_node_->get_num_entries();
					lptr_[i].leaf_pos_ = num_entries - 1;
					lptr_[i].size_ = num_entries - pos * increment;
				}
				else {
					lptr_[i].leaf_pos_ = pos * increment + increment - 1;
					lptr_[i].size_ = increment;
				}
				page_io_++;

				// -----------------------------------------------------------------
				//  init right buffer
				// -----------------------------------------------------------------
				if (pos < lptr_[i].leaf_node_->get_num_keys() - 1) {
					rptr_[i].leaf_node_ = lptr_[i].leaf_node_;
					rptr_[i].index_pos_ = (pos + 1);
					rptr_[i].leaf_pos_  = (pos + 1) * increment;

					if ((pos + 1) == rptr_[i].leaf_node_->get_num_keys() - 1) {
						num_entries = rptr_[i].leaf_node_->get_num_entries();
						rptr_[i].size_ = num_entries - (pos + 1) * increment;
					}
					else {
						rptr_[i].size_ = increment;
					}
				}
				else {
					rptr_[i].leaf_node_ = lptr_[i].leaf_node_->get_right_sibling();
					if (rptr_[i].leaf_node_) {
						rptr_[i].index_pos_ = 0;
						rptr_[i].leaf_pos_ = 0;

						increment = rptr_[i].leaf_node_->get_increment();
						num_entries = rptr_[i].leaf_node_->get_num_entries();
						if (increment > num_entries) {
							rptr_[i].size_ = num_entries;
						}
						else {
							rptr_[i].size_ = increment;
						}
						page_io_++;
					}
				}
			}
		}
		else {
			// ---------------------------------------------------------------------
			//  only one level in the B+ Tree: one lead node
			//  (1) init left buffer
			// ---------------------------------------------------------------------
			lptr_[i].leaf_node_ = new BLeafNode();
			lptr_[i].leaf_node_->init_restore(trees_[i], block);

			pos = lptr_[i].leaf_node_->find_position_by_key(q_val_[i]);
			if (pos < 0) pos = 0;
			lptr_[i].index_pos_ = pos;

			increment = lptr_[i].leaf_node_->get_increment();
			if (pos == lptr_[i].leaf_node_->get_num_keys() - 1) {
				num_entries = lptr_[i].leaf_node_->get_num_entries();
				lptr_[i].leaf_pos_ = num_entries - 1;
				lptr_[i].size_ = num_entries - pos * increment;
			}
			else {
				lptr_[i].leaf_pos_ = pos * increment + increment - 1;
				lptr_[i].size_ = increment;
			}
			page_io_++;
			
			// ---------------------------------------------------------------------
			//  (2) init right buffer
			// ---------------------------------------------------------------------
			if (pos < lptr_[i].leaf_node_->get_num_keys() - 1) {
				rptr_[i].leaf_node_ = lptr_[i].leaf_node_;
				rptr_[i].index_pos_ = (pos + 1);
				rptr_[i].leaf_pos_  = (pos + 1) * increment;

				if ((pos + 1) == rptr_[i].leaf_node_->get_num_keys() - 1) {
					num_entries = rptr_[i].leaf_node_->get_num_entries();
					rptr_[i].size_ = num_entries - (pos + 1) * increment;
				}
				else {
					rptr_[i].size_ = increment;
				}
			}
			else {
				rptr_[i].leaf_node_ = NULL;
				rptr_[i].index_pos_ = -1;
				rptr_[i].leaf_pos_ = -1;
				rptr_[i].size_ = -1;
			}
		}

		if (index_node != NULL) {
			delete index_node; index_node = NULL;
		}
	}
}

// -----------------------------------------------------------------------------
float QALSH::find_radius(			// find proper radius
	const float *q_val,					// hash value of query
	const PageBuffer *lptr,				// left page buffer
	const PageBuffer *rptr)				// right page buffer
{
	float radius = update_radius(1.0f / appr_ratio_, q_val, lptr, rptr);
	if (radius < 1.0f) radius = 1.0f;

	return radius;
}

// -----------------------------------------------------------------------------
float QALSH::update_radius(			// update radius
	float old_radius,					// old radius
	const float *q_val,					// hash value of query
	const PageBuffer *lptr,				// left page buffer
	const PageBuffer *rptr)				// right page buffer
{
	float dist = 0.0f;
	vector<float> list;

	for (int i = 0; i < m_; ++i) {	// find an array of proj dist
		if (lptr[i].size_ != -1) {
			dist = calc_dist(q_val[i], &lptr[i]);
			list.push_back(dist);
		}
		if (rptr[i].size_ != -1) {
			dist = calc_dist(q_val[i], &rptr[i]);
			list.push_back(dist);
		}
	}
	sort(list.begin(), list.end());	// sort the array

	int num = (int)list.size();
	if (num == 0) return appr_ratio_ * old_radius;

	if (num % 2 == 0) {				// find median dist
		dist = (list[num / 2 - 1] + list[num / 2]) / 2.0f;
	}
	else {
		dist = list[num / 2];
	}
	list.clear();

	int kappa = (int)ceil(log(2.0f * dist / w_) / log(appr_ratio_));
	dist = pow(appr_ratio_, kappa);

	return dist;
}

// -----------------------------------------------------------------------------
void QALSH::update_left_buffer(		// update left buffer
	const PageBuffer *rptr,				// right buffer
	PageBuffer *lptr)					// left buffer (return)
{
	BLeafNode *leaf_node = NULL;
	BLeafNode *old_leaf_node = NULL;

	if (lptr->index_pos_ > 0) {
		lptr->index_pos_--;

		int pos = lptr->index_pos_;
		int increment = lptr->leaf_node_->get_increment();
		lptr->leaf_pos_ = pos * increment + increment - 1;
		lptr->size_ = increment;
	}
	else {
		old_leaf_node = lptr->leaf_node_;
		leaf_node = lptr->leaf_node_->get_left_sibling();

		if (leaf_node) {
			lptr->leaf_node_ = leaf_node;
			lptr->index_pos_ = lptr->leaf_node_->get_num_keys() - 1;

			int pos = lptr->index_pos_;
			int increment = lptr->leaf_node_->get_increment();
			int num_entries = lptr->leaf_node_->get_num_entries();
			lptr->leaf_pos_ = num_entries - 1;
			lptr->size_ = num_entries - pos * increment;
			page_io_++;
		}
		else {
			lptr->leaf_node_ = NULL;
			lptr->index_pos_ = -1;
			lptr->leaf_pos_ = -1;
			lptr->size_ = -1;
		}

		if (rptr->leaf_node_ != old_leaf_node) {
			delete old_leaf_node; old_leaf_node = NULL;
		}
	}
}

// -----------------------------------------------------------------------------
void QALSH::update_right_buffer(	// update right buffer
	const PageBuffer *lptr,				// left buffer
	PageBuffer *rptr)					// right buffer (return)
{
	BLeafNode *leaf_node = NULL;
	BLeafNode *old_leaf_node = NULL;

	if (rptr->index_pos_ < rptr->leaf_node_->get_num_keys() - 1) {
		rptr->index_pos_++;

		int pos = rptr->index_pos_;
		int increment = rptr->leaf_node_->get_increment();
		rptr->leaf_pos_ = pos * increment;
		if (pos == rptr->leaf_node_->get_num_keys() - 1) {
			int num_entries = rptr->leaf_node_->get_num_entries();
			rptr->size_ = num_entries - pos * increment;
		}
		else {
			rptr->size_ = increment;
		}
	}
	else {
		old_leaf_node = rptr->leaf_node_;
		leaf_node = rptr->leaf_node_->get_right_sibling();

		if (leaf_node) {
			rptr->leaf_node_ = leaf_node;
			rptr->index_pos_ = 0;
			rptr->leaf_pos_ = 0;

			int increment = rptr->leaf_node_->get_increment();
			int num_entries = rptr->leaf_node_->get_num_entries();
			if (increment > num_entries) {
				rptr->size_ = num_entries;
			}
			else {
				rptr->size_ = increment;
			}
			page_io_++;
		}
		else {
			rptr->leaf_node_ = NULL;
			rptr->index_pos_ = -1;
			rptr->leaf_pos_ = -1;
			rptr->size_ = -1;
		}

		if (lptr->leaf_node_ != old_leaf_node) {
			delete old_leaf_node; old_leaf_node = NULL;
		}
	}
}

// -----------------------------------------------------------------------------
float QALSH::calc_dist(				// calc projected distance
	float q_val,						// hash value of query
	const PageBuffer *ptr)				// page buffer
{
	int pos = ptr->index_pos_;
	float key = ptr->leaf_node_->get_key(pos);
	float dist = fabs(key - q_val);

	return dist;
}

// -----------------------------------------------------------------------------
void QALSH::delete_tree_ptr()		// delete the pointers of B+ Trees
{
	for (int i = 0; i < m_; ++i) {
		// ---------------------------------------------------------------------
		//  CANNOT remove the condition
		//              <lptrs[i].leaf_node != rptrs[i].leaf_node>
		//  because <lptrs[i].leaf_node> and <rptrs[i].leaf_node> may point 
		//  to the same address, then we would delete it twice and receive 
		//  the runtime error or segmentation fault.
		// ---------------------------------------------------------------------
		if (lptr_[i].leaf_node_ && lptr_[i].leaf_node_ != rptr_[i].leaf_node_) {
			delete lptr_[i].leaf_node_; lptr_[i].leaf_node_ = NULL;
		}
		if (rptr_[i].leaf_node_) {
			delete rptr_[i].leaf_node_; rptr_[i].leaf_node_ = NULL;
		}
	}
}
