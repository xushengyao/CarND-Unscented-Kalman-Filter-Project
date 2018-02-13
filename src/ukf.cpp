#include "ukf.h"
#include "Eigen/Dense"
#include <iostream>

using namespace std;
using Eigen::MatrixXd;
using Eigen::VectorXd;
using std::vector;

/**
 * Initializes Unscented Kalman filter
 * This is scaffolding, do not modify
 */
UKF::UKF() {
  // if this is false, laser measurements will be ignored (except during init)
  use_laser_ = true;

  // if this is false, radar measurements will be ignored (except during init)
  use_radar_ = true;

  // initial state vector
  x_ = VectorXd(5);

  // initial covariance matrix
  P_ = MatrixXd(5, 5);

  // Process noise standard deviation longitudinal acceleration in m/s^2
  std_a_ = 2;

  // Process noise standard deviation yaw acceleration in rad/s^2
  std_yawdd_ = 0.3;

  //DO NOT MODIFY measurement noise values below these are provided by the sensor manufacturer.
  // Laser measurement noise standard deviation position1 in m
  std_laspx_ = 0.15;

  // Laser measurement noise standard deviation position2 in m
  std_laspy_ = 0.15;

  // Radar measurement noise standard deviation radius in m
  std_radr_ = 0.3;

  // Radar measurement noise standard deviation angle in rad
  std_radphi_ = 0.03;

  // Radar measurement noise standard deviation radius change in m/s
  std_radrd_ = 0.3;
  //DO NOT MODIFY measurement noise values above these are provided by the sensor manufacturer.

  /**
  TODO:

  Complete the initialization. See ukf.h for other member properties.

  Hint: one or more values initialized above might be wildly off...
  */
  //set initialize state
  is_initialized_ = false;
  //initiate time
  time_us_ = 0.0;
  //set state dimension
  n_x_ = 5;
  //set augmented dimension
  n_aug_ = 7;
  //define spreading parameter
  lambda_ = 3 - n_x_;
  //sigma points matrix
  Xsig_pred_ = MatrixXd(n_x_, 2 * n_aug_ + 1);
  //weights
  weights_ = VectorXd(2 * n_aug_ + 1);
  //NIS
  NIS_radar_ = 0.0;
  NIS_laser_ = 0.0;
}

UKF::~UKF() {}

/**
 * @param {MeasurementPackage} meas_package The latest measurement data of
 * either radar or laser.
 */
void UKF::ProcessMeasurement(MeasurementPackage meas_package) {
  /**
  TODO:

  Complete this function! Make sure you switch between lidar and radar
  measurements.
  */
  if(!is_initialized_){

    time_us_ = meas_package.timestamp_;

  //  P_ = MatrixXd::Identity(5,5);
    //P_(0,0)=0.5;
    //P_(1,1)=0.5;
    x_ << 1, 1, 1, 1, 0.1;

    // init covariance matrix
    P_ << 0.15,    0, 0, 0, 0,
             0, 0.15, 0, 0, 0,
             0,    0, 1, 0, 0,
             0,    0, 0, 1, 0,
             0,    0, 0, 0, 1;
    if ((meas_package.sensor_type_ == MeasurementPackage::RADAR) && use_radar_){
      /**
      Convert radar from polar to cartesian coordinates and initialize state.
      */

      double ro = meas_package.raw_measurements_[0];
      double phi = meas_package.raw_measurements_[1];
      double ro_dot = meas_package.raw_measurements_[2];

      double px = ro * cos(phi);
      double py = ro * sin(phi);
      double vx = ro_dot * cos(phi);
      double vy = ro_dot * sin(phi);
      double v  = sqrt(vx * vx + vy * vy);

      x_(0) = px;
      x_(1) = py;
      x_(2) = v;
      x_(3) = 0.0;
      x_(4) = 0.0;
    }
    else if ((meas_package.sensor_type_ == MeasurementPackage::LASER) && use_laser_) {
      /**
      Initialize state.
      */
      x_(0) = meas_package.raw_measurements_(0);
      x_(1) = meas_package.raw_measurements_(1);
      x_(2) = 0.0;
      x_(3) = 0.0;
      x_(4) = 0.0;
    }
    is_initialized_ = true;
    return;
  }

  //compute the time elapsed between the current and previous measurements
  double dt = (meas_package.timestamp_ - time_us_) / 1000000.0;

  //if (dt > 0.001) {
  time_us_ = meas_package.timestamp_;
  Prediction(dt);
  //}

  if ((meas_package.sensor_type_ == MeasurementPackage::LASER) && use_laser_) {
    UpdateLidar(meas_package);
  } else if ((meas_package.sensor_type_ == MeasurementPackage::RADAR) && use_radar_) {
    UpdateRadar(meas_package);
  }
}

/**
 * Predicts sigma points, the state, and the state covariance matrix.
 * @param {double} delta_t the change in time (in seconds) between the last
 * measurement and this one.
 */
void UKF::Prediction(double delta_t) {
  /**
  TODO:

  Complete this function! Estimate the object's location. Modify the state
  vector, x_. Predict sigma points, the state, and the state covariance matrix.
  */
  //create sigma point matrix
  MatrixXd Xsig = MatrixXd(n_x_, 2 * n_x_ + 1);

  //calculate square root of P
  MatrixXd A = P_.llt().matrixL();

  //set lambda for non-augmented sigma points
  lambda_ = 3 - n_x_;

  //set first column of sigma point matrix
  Xsig.col(0) = x_;

  //set remaining sigma points
  for (int i = 0; i < n_x_; i++)
  {
    Xsig.col(i + 1) = x_ + sqrt(lambda_ + n_x_) * A.col(i);
    Xsig.col(i + 1 + n_x_) = x_ - sqrt(lambda_ + n_x_) * A.col(i);
  }

  //create augmented mean vector
  VectorXd x_aug = VectorXd(n_aug_);

  //create augmented state covariance
  MatrixXd P_aug = MatrixXd(n_aug_, n_aug_);

  //create sigma point matrix
  MatrixXd Xsig_aug = MatrixXd(n_aug_, 2 * n_aug_ + 1);

  //set lambda for augmented sigma points
  lambda_ = 3 - n_aug_;
  //create augmented mean state
  x_aug.head(5) = x_;
  x_aug(5) = 0;
  x_aug(6) = 0;

  //create augmented covariance matrix
  P_aug.fill(0.0);
  P_aug.topLeftCorner(5,5) = P_;
  P_aug(5,5) = std_a_*std_a_;
  P_aug(6,6) = std_yawdd_*std_yawdd_;

  //create square root matrix
  MatrixXd L = P_aug.llt().matrixL();

  //create augmented sigma points
  Xsig_aug.col(0)  = x_aug;
  for (int i = 0; i< n_aug_; i++)
  {
    Xsig_aug.col(i+1)       = x_aug + sqrt(lambda_+n_aug_) * L.col(i);
    Xsig_aug.col(i+1+n_aug_) = x_aug - sqrt(lambda_+n_aug_) * L.col(i);
  }

  //predict sigma points
  for (int i = 0; i< 2*n_aug_+1; i++)
  {
    //extract values for better readability
    double p_x = Xsig_aug(0,i);
    double p_y = Xsig_aug(1,i);
    double v = Xsig_aug(2,i);
    double yaw = Xsig_aug(3,i);
    double yawd = Xsig_aug(4,i);
    double nu_a = Xsig_aug(5,i);
    double nu_yawdd = Xsig_aug(6,i);
    //predicted state values
    double px_p, py_p;
    //avoid division by zero
    if (fabs(yawd) > 0.001) {
       px_p = p_x + v/yawd * ( sin (yaw + yawd*delta_t) - sin(yaw));
       py_p = p_y + v/yawd * ( cos(yaw) - cos(yaw+yawd*delta_t) );
    }
    else {
       px_p = p_x + v*delta_t*cos(yaw);
       py_p = p_y + v*delta_t*sin(yaw);
    }
    double v_p = v;
    double yaw_p = yaw + yawd*delta_t;
    double yawd_p = yawd;

    //add noise
    px_p = px_p + 0.5*nu_a*delta_t*delta_t * cos(yaw);
    py_p = py_p + 0.5*nu_a*delta_t*delta_t * sin(yaw);
    v_p = v_p + nu_a*delta_t;

    yaw_p = yaw_p + 0.5*nu_yawdd*delta_t*delta_t;
    yawd_p = yawd_p + nu_yawdd*delta_t;

    //write predicted sigma point into right column
    Xsig_pred_(0,i) = px_p;
    Xsig_pred_(1,i) = py_p;
    Xsig_pred_(2,i) = v_p;
    Xsig_pred_(3,i) = yaw_p;
    Xsig_pred_(4,i) = yawd_p;
   }
     // set weights
   double weight_0 = lambda_ / (lambda_ + n_aug_);
   weights_(0) = weight_0;
   for (int i = 1; i < 2 * n_aug_ + 1; i++) {
     double weight = 0.5 / (n_aug_ + lambda_);
     weights_(i) = weight;
   }


   //predicted state mean
   x_.fill(0.0);
   for (int i = 0; i < 2 * n_aug_ + 1; i++) {
     x_ = x_+ weights_(i) * Xsig_pred_.col(i);
   }

   //predicted state covariance matrix
   P_.fill(0.0);
   for (int i = 0; i < 2 * n_aug_ + 1; i++) {

     // state difference
     VectorXd x_diff = Xsig_pred_.col(i) - x_;
     //angle normalization
     while (x_diff(3)> M_PI) x_diff(3)-=2.*M_PI;
     while (x_diff(3)<-M_PI) x_diff(3)+=2.*M_PI;

     P_ = P_ + weights_(i) * x_diff * x_diff.transpose() ;
   }
 }

void UKF::UpdateLidar(MeasurementPackage meas_package) {
  /**
  TODO:

  Complete this function! Use lidar data to update the belief about the object's
  position. Modify the state vector, x_, and covariance, P_.

  You'll also need to calculate the lidar NIS.
  */
  VectorXd z = meas_package.raw_measurements_;

  //set measurement dimension, lidar can measure p_x and p_y
  int n_z = 2;

  //create matrix for sigma points in measurement namespace
  MatrixXd Zsig = MatrixXd(n_z, 2 * n_aug_+1);

  for (int i = 0; i< 2 * n_aug_ + 1; i++){
    double p_x = Xsig_pred_(0,i);
    double p_y = Xsig_pred_(1,i);

    // measurement model
    Zsig(0,i) = p_x;
    Zsig(1,i) = p_y;
  }

  //mean predicted measurement
  VectorXd z_pred = VectorXd(n_z);
  z_pred.fill(0.0);
  for (int i = 0; i < 2 * n_aug_; i++){
    z_pred = z_pred + weights_(i) * Zsig.col(i);
  }
  //measurement covariance matrix S
  MatrixXd S = MatrixXd(n_z, n_z);
  S.fill(0.0);
  for (int i = 0; i < 2 * n_aug_ + 1; i++){
    //residuals
    VectorXd z_diff = Zsig.col(i) - z_pred;

    S = S + weights_(i) * z_diff * z_diff.transpose();
  }

  // add measuremtn noise covariance Matrix
  MatrixXd R = MatrixXd(n_z, n_z);
  R << std_laspx_*std_laspx_, 0,
       0, std_laspy_*std_laspy_;
  S = S + R;

  //create matrix for cross correlation Tc
  MatrixXd Tc = MatrixXd(n_x_, n_z);
  Tc.fill(0.0);
  for (int i = 0; i < 2 * n_aug_ + 1; i++){
    // residual
    VectorXd z_diff = Zsig.col(i) - z_pred;
    VectorXd x_diff = Xsig_pred_.col(i) - x_;

    Tc = Tc + weights_(i) * x_diff * z_diff.transpose();
  }

  //Kalman gain K;
  MatrixXd K = Tc * S.inverse();

  //residual
  VectorXd z_diff = z - z_pred;

  //NIS_laser
  NIS_laser_ = z_diff.transpose() * S.inverse() * z_diff;

  //update state mean and covariance matrix
  x_ = x_ + K * z_diff;
  P_ = P_ - K*S*K.transpose();
}

/**
 * Updates the state and the state covariance matrix using a radar measurement.
 * @param {MeasurementPackage} meas_package
 */
void UKF::UpdateRadar(MeasurementPackage meas_package) {
  /**
  TODO:

  Complete this function! Use radar data to update the belief about the object's
  position. Modify the state vector, x_, and covariance, P_.

  You'll also need to calculate the radar NIS.
  */
  VectorXd z = meas_package.raw_measurements_;

  //set measurement dimension, radar can measure r, phi, and r_dot
  int n_z = 3;

  //create matrix for sigma points in measurement space
  MatrixXd Zsig = MatrixXd(n_z, 2 * n_aug_ + 1);

  //transform sigma points into measurement space
  for (int i = 0; i < 2 * n_aug_ + 1; i++) {
    // extract values for better readibility
    double p_x = Xsig_pred_(0, i);
    double p_y = Xsig_pred_(1, i);
    double v   = Xsig_pred_(2, i);
    double yaw = Xsig_pred_(3, i);

    double v1 = cos(yaw)*v;
    double v2 = sin(yaw)*v;

    // measurement model
    Zsig(0, i) = sqrt(p_x*p_x + p_y*p_y);                        //r
    Zsig(1, i) = atan2(p_y, p_x);                                //phi
    Zsig(2, i) = (p_x*v1 + p_y*v2) / sqrt(p_x*p_x + p_y*p_y);   //r_dot
  }
  //mean predicted measurement
  VectorXd z_pred = VectorXd(n_z);
  z_pred.fill(0.0);
  for (int i = 0; i < 2 * n_aug_ + 1; i++) {
    z_pred = z_pred + weights_(i) * Zsig.col(i);
  }
  //measurement covariance matrix S
  MatrixXd S = MatrixXd(n_z, n_z);
  S.fill(0.0);
  for (int i = 0; i < 2 * n_aug_ + 1; i++) {
    //residual
    VectorXd z_diff = Zsig.col(i) - z_pred;

    //angle normalization
    while (z_diff(1)> M_PI) z_diff(1) -= 2.*M_PI;
    while (z_diff(1)<-M_PI) z_diff(1) += 2.*M_PI;

    S = S + weights_(i) * z_diff * z_diff.transpose();
   }

   //add measurement noise covariance matrix
   MatrixXd R = MatrixXd(n_z, n_z);
   R << std_radr_*std_radr_, 0, 0,
        0, std_radphi_*std_radphi_, 0,
        0, 0,std_radrd_*std_radrd_;
   S = S + R;

   //create matrix for cross correlation Tc
   MatrixXd Tc = MatrixXd(n_x_, n_z);

   //calculate cross correlation matrix
   Tc.fill(0.0);
   for (int i = 0; i < 2 * n_aug_ + 1; i++) {
     //residual
     VectorXd z_diff = Zsig.col(i) - z_pred;
     //angle normalization
     while (z_diff(1)> M_PI) z_diff(1) -= 2.*M_PI;
     while (z_diff(1)<-M_PI) z_diff(1) += 2.*M_PI;

     // state difference
     VectorXd x_diff = Xsig_pred_.col(i) - x_;
     //angle normalization
     while (x_diff(3)> M_PI) x_diff(3) -= 2.*M_PI;
     while (x_diff(3)<-M_PI) x_diff(3) += 2.*M_PI;

     Tc = Tc + weights_(i) * x_diff * z_diff.transpose();
   }

   //Kalman gain K;
   MatrixXd K = Tc * S.inverse();

   //residual
   VectorXd z_diff = z - z_pred;

   //angle normalization
   while (z_diff(1)> M_PI) z_diff(1) -= 2.*M_PI;
   while (z_diff(1)<-M_PI) z_diff(1) += 2.*M_PI;

   //calculate NIS
   NIS_radar_ = z_diff.transpose() * S.inverse() * z_diff;

   //update yjr state mean and covariance matrix
   x_ = x_ + K * z_diff;
   P_ = P_ - K*S*K.transpose();
}
