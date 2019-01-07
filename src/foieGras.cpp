#include <TMB.hpp>

/*
	Random walk with velocities as a random walk.
	Kind of a correlated RW model - with code for KF and LS argos errors

	Authors:
		Ian Jonsen 	(ian.jonsen@mq.edu.au)
		Toby Patterson (Toby.Patterson@csiro.au)
*/

using namespace density;

template<class Type>
  Type objective_function<Type>::operator() ()
  {
    // DATA
    DATA_MATRIX(Y);	            //  (x, y) observations
    DATA_VECTOR(dt);         	//  time diff in some appropriate unit. this should contain dt for both interp and obs positions.
    DATA_VECTOR(state0);        //  initial state
    DATA_IVECTOR(isd);          //  indexes observations vs. interpolation points
    DATA_IVECTOR(obs_mod);      //  indicates which obs error model to be used
    DATA_INTEGER(proc_mod);		//	indicates which process model to be used: RW or CRW

    // for KF observation model
    DATA_VECTOR(m);             //  m is the semi-minor axis length
    DATA_VECTOR(M);             //  M is the semi-major axis length
    DATA_VECTOR(c);             //  c is the orientation of the error ellipse
    // for LS observation model
    DATA_MATRIX(K);                 // error weighting factors for LS obs model

    // PROCESS PARAMETERS
    // for RW
    PARAMETER_VECTOR(l_sigma);    //  Innovation variance (link scale)
    PARAMETER(l_rho_p);           //  Innovation correlation (link scale)
    PARAMETER_MATRIX(X);          //  Predicted locations TP - length(X) should be same as length(dt) - i.e. both interp & obs pos.
    // for CRW
    PARAMETER(logD);				// 1-d Diffusion coefficient
    // random variables
    PARAMETER_MATRIX(mu); /* State location */
    PARAMETER_ARRAY(v); /* state velocities */

    // OBSERVATION PARAMETERS
    // for KF OBS MODEL
    PARAMETER(l_psi); 				// error SD scaling parameter to account for possible uncertainty in Argos error ellipse variables
    // for LS OBS MODEL
    PARAMETER_VECTOR(l_tau);     	// error dispersion for LS obs model (log scale)
    PARAMETER(l_rho_o);             // error correlation

	  // Transform parameters
	  vector<Type> sigma = exp(l_sigma);
    Type rho_p = Type(2.0) / (Type(1.0) + exp(-l_rho_p)) - Type(1.0);
    vector<Type> tau = exp(l_tau);
    Type rho_o = Type(2.0) / (Type(1.0) + exp(-l_rho_o)) - Type(1.0);
    Type psi = exp(l_psi);
    Type D = exp(logD);

	int timeSteps = X.rows();
	/* Define likelihood */
    parallel_accumulator<Type> jnll(this);
    Type tiny = 1e-5;

    if(proc_mod == 0) {
    	// RW
    	// 2 x 2 covariance matrix for innovations
    	matrix<Type> cov(2, 2);
    	matrix<Type> cov_dt(2, 2);            // tmp matrix for dt * cov calcs withn process loop

    	cov(0, 0) = sigma(0) * sigma(0);
    	cov(0, 1) = rho_p * sigma(0) * sigma(1);
    	cov(1, 0) = cov(0, 1);
    	cov(1, 1) = sigma(1) * sigma(1);

		MVNORM_t<Type> nll_proc(cov);

    	// RW PROCESS MODEL
    	for(int i = 1; i < timeSteps; i++) {
      		cov_dt = pow(dt(i), 2) * cov;
      		nll_proc.setSigma(cov_dt);
      		jnll += nll_proc(X.row(i) - X.row(i - 1));
    	}
    } else if(proc_mod == 1){
    	// CRW
		// Setup object for evaluating multivariate normal likelihood
    	matrix<Type> cov(4,4);
    	cov.setZero();
    	cov(0,0) = tiny;
    	cov(1,1) = tiny;
    	cov(2,2) = 2 * D * dt(0);
    	cov(3,3) = 2 * D * dt(0);

    	// loop over 2 coords and update nll of start location and velocities.
    	for(int i=0;i<2;i++) {
    	  jnll -= dnorm(mu(i,0), state0(i), tiny, true);
    	  jnll -= dnorm(v(i,0), state0(i+2), tiny, true);
    	}

    	// CRW PROCESS MODEL
    	vector<Type> x_t(4);
    	for(int i=1;i<timeSteps;i++) {
    		// process cov at time t
      		cov.setZero();
      		cov(0,0) = tiny;
      		cov(1,1) = tiny;
      		cov(2,2) = 2 * D * dt(i);
      		cov(3,3) = 2 * D * dt(i);

      		// location innovations
      		x_t(0) = mu(0,i) - (mu(0,i-1) + (v(0,i) * dt(i)));
      		x_t(1) = mu(1,i) - (mu(1,i-1) + (v(1,i) * dt(i)));

      		// velocity innovations
      		x_t(2) = (v(0,i) - v(0,i-1)); // /del_t(i); // divide by del_t(i) or not?
      		x_t(3) = (v(1,i) - v(1,i-1)); // /del_t(i);
      		jnll += MVNORM<Type>(cov)(x_t); // Process likelihood
    	}
    }

    // OBSERVATION MODEL
    // 2 x 2 covariance matrix for observations
    matrix<Type> cov_obs(2, 2);
    MVNORM_t<Type> nll_obs; // Multivariate Normal for observations

//    vector<Type> mu_t(2);
    for(int i=0; i < timeSteps; ++i) {
      if(isd(i) == 1) {
        if(obs_mod(i) == 0) {
          // Argos Least Squares observations
          Type s = tau(0) * K(i,0);
          Type q = tau(1) * K(i,1);
          cov_obs(0,0) = pow(s, 2);
          cov_obs(1,1) = pow(q, 2);
          cov_obs(0,1) = s * q * rho_o;
          cov_obs(1,0) = cov_obs(0,1);
        } else if(obs_mod(i) == 1) {
          // Argos Kalman Filter (or Kalman Smoothed) observations using Mclintock formulae
          Type s2c = sin(c(i)) * sin(c(i));
          Type c2c = cos(c(i)) * cos(c(i));
          Type M2  = (M(i) / sqrt(2)) * (M(i) / sqrt(2));
          Type m2 = (m(i) * psi / sqrt(2)) * (m(i) * psi / sqrt(2));
          cov_obs(0,0) = (M2 * s2c + m2 * c2c);
          cov_obs(1,1) = (M2 * c2c + m2 * s2c);
          cov_obs(0,1) = (0.5 * (pow(M(i),2) - pow(m(i) * psi,2))) * cos(c(i)) * sin(c(i));
          cov_obs(1,0) = cov_obs(0,1);
        }
        nll_obs.setSigma(cov_obs);   // set up i-th obs cov matrix
//        mu_t(0) = (Y(0,i) - mu(0,i));
//        mu_t(1) = (Y(1,i) - mu(1,i));
        if(proc_mod == 0) {
          jnll += nll_obs(Y.row(i) - X.row(i));   // RW innovations
        } else if(proc_mod == 1) {
          jnll += nll_obs(Y.row(i) - mu.row(i));   // CRW innovations
        }

      }
    }

    ADREPORT(rho_p);
    ADREPORT(sigma);
    ADREPORT(D);
    ADREPORT(rho_o);
    ADREPORT(tau);
    ADREPORT(psi);

    return jnll;
}

