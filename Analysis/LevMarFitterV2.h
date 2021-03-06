/* Copyright (C) 2010 Ion Torrent Systems, Inc. All Rights Reserved */
#ifndef LEVMARFITTERV2_H
#define LEVMARFITTERV2_H

//#include <armadillo>
#include <string.h>
#include <stdlib.h>
#include <float.h>
#include <cblas.h>

// some of the code uses <complex.h>, and in <complex.h> 'I' is defined and this
// interferes w/ lapackpp.  I undef it here in case anyone above has included <complex.h>
#undef I
#define LEVMAR_STEP_V2 10.0
#define REGULARIZER_VAL 0.0000001

class BkgFitLevMarDat;
// base class for fitting algorithms
class LevMarFitterV2
{

  public:

    int Fit (int max_iter, float *y, float *params, float *std_err = NULL);
    // evaluate the fitted function w/ the specified parameters
    virtual void Evaluate (float *y) = 0;

    // evaluate the fitted function w/ the specified parameters
    virtual void Evaluate (float *y,float *params) = 0;

    ~LevMarFitterV2();

    // fit the data
    virtual int Fit (int max_iter,float *y) = 0;

    void SetLambdaThreshold (float _lambda_threshold);

    void SetLambdaStart (float _initial_lambda);

    float GetLambda (void);
    void SetDataDeltaNull();
    double GetDataDelta (int);

    void SetDebugTrace (bool _enable);
    // set the weighting vector
    void SetWeightVector (float *vect);
    // set prior values for parameters
    void SetPrior (float *input_prior);
    // dampening effect of prior for each parameter
    void SetDampers (float *input_dampers);

    // get the mean squared error after the fit
    float GetMeanSquaredError (float *y = NULL, bool use_fval_cache = false);
    void ReturnPredicted(float *f_predict, bool use_fval_cache);
    int getNumException()
    {
      return numException;
    };
    virtual int GetnParams()
    {
      return nparams;
    }
    void SetFvalCacheEnable (bool enable)
    {
      enable_fval_cache = enable;
      enable_fval_cache = false;
    }
    
    void SetParamMax (float *_param_max);
    void SetParamMin (float *_param_min);
    void SetParamVal (float *_param_val);
    float ReturnNthParam(int idx);
    void SetNthParam(float val, int idx);
    
  protected:
    LevMarFitterV2() : npts (0), x (NULL), residual (0.0), dp (NULL), residualWeight (NULL),
        prior (NULL), dampers (NULL), wtScale (0.0), nparams (0), param_val(NULL) , param_max (NULL), param_min (NULL), debug_trace (false),
        fval_cache (NULL), enable_fval_cache (false), lambda (1.0), lambda_threshold (1.0E+10),regularizer(0.0),data (NULL),numException (0)
    {
    }

    void Initialize (int _nparams,int _npts,float *_x);

    virtual float CalcResidual (float *refVals,
                                float *testVals,
                                int numVals,
                                float *err_vec = NULL)
    {
      double r = 0.0;
      double e;

      if (err_vec)
        for (int i=0;i < numVals;i++)
        {
          //e = (*err_vec)(i) = residualWeight[i] * (refVals[i] - testVals[i]);
          err_vec[i] = e = residualWeight[i] * (refVals[i] - testVals[i]);
          r += e*e;
        }
      else
        for (int i=0;i < numVals;i++)
        {
          e = residualWeight[i] * (refVals[i] - testVals[i]);
          r += e*e;
        }

      // r = sqrt(r/wtScale)
      r = (r/wtScale);
      return r;
    }

// virtual - >may< quit early if we have good reason
    virtual bool DoneTest (int iter,
                           int max_iter,
                           BkgFitLevMarDat *data,
                           float lambda,
                           int &done_cnt,
                           float residual,
                           float r_chg)
    {
      (void) residual;
      //(void) data->delta;
      SetDataDeltaNull();
      (void) lambda;

      if (r_chg < 0.0) done_cnt = 0;
      else done_cnt++;

      return ( (iter >= max_iter) || (done_cnt >= 5));
    }
    
    // in case someone over-rides the done test and forgets to be smart
    // like we did by not including a lambda_threshold test
    bool ForceQuit(int iter, int max_iter, float lambda)
    {
      return((iter>=max_iter) or (lambda>lambda_threshold));  // no virtual function, >must< quit if we escape bounds
    }

    // generic grid search
    void GridSearch (int steps,float *y,float *params);
    virtual void ApplyMoveConstraints (float *params_new)
    {
      for (int i=0; i< nparams; i++)
      {
        // apply limits if necessary
        if (param_max) params_new[i] = (params_new[i] > param_max[i] ? param_max[i] : params_new[i]);
        if (param_min) params_new[i] = (params_new[i] < param_min[i] ? param_min[i] : params_new[i]);
      }
    }

    float EvaluateParamFromPrior (float *param_new);
    void TryLevMarStep (float *y, float *fval, float *params, float *err_vect, double *bfjtj, double *bfrhs, int done_cnt, float &r_start, float &r_chg);

    // allow replacement
    virtual void MakeJacobian (float *bfjac, float *params, float *fval)
    {
      float tmp[npts];
      float params_new[nparams];
      // evaluate the partial derivatives w.r.t. each parameter
      memcpy (params_new,params,sizeof (float[nparams]));
      for (int i=0;i < nparams;i++)
      {
        // adjust parameter
        params_new[i] += dp[i];

        // re-evaluate function
        Evaluate (tmp,params_new);

        // store in jacobian
        for (int j=0;j < npts;j++)
          bfjac[j*nparams+i] = residualWeight[j] * (tmp[j]-fval[j]) / dp[i];

        // put param back
        params_new[i] = params[i];

      }
    }

    void CalculateJTJ (double *bfjtj, float *bfjac);
    void CalculateRHS (double *bfrhs, float *bfjac, float *err_vect, float *params);

    int npts;
    float *x;
    float residual;
    float *dp;
    float *residualWeight;
    float *prior;
    float *dampers;
    float wtScale;
    
    int nparams;
    float *param_val;
    float *param_max;
    float *param_min;

    
    bool  debug_trace;
    float *fval_cache;
    bool enable_fval_cache;

  private:
    float lambda;
    float lambda_threshold;
    float regularizer;

    BkgFitLevMarDat *data;
    int numException;

};


#endif // LEVMARFITTERV2_H

