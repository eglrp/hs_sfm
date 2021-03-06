#include <iostream>

#include <gtest/gtest.h>

#include "hs_test_utility/test_monte_carlo/normal_mle_simulator.hpp"

#include "hs_sfm/bundle_adjustment/ba_naive_vector_function.hpp"
#include "hs_sfm/bundle_adjustment/ba_naive_synthetic_data_generator.hpp"
#include "hs_sfm/bundle_adjustment/ba_naive_noised_x_generator.hpp"

#include "test_ba_naive_normal_mle_meta.hpp"

namespace
{

template <typename _Scalar>
class TestBANaiveMonteCarloSimulate
{
public:
  typedef _Scalar Scalar;
  typedef int Err;
  typedef hs::sfm::ba::BANaiveVectorFunction<Scalar> VectorFunction;
  typedef typename VectorFunction::Index Index;
  typedef
    hs::sfm::ba::BANaiveForwardFiniteDifferenceJacobianMatrixCalculator<
      VectorFunction> JacobianMatrixCalculator;
  typedef typename JacobianMatrixCalculator::JacobianMatrix JacobianMatrix;
  typedef hs::test::mc::NormalMLESimulator<VectorFunction> Simulator;
  typedef typename Simulator::NoisedYVectorGenerator
    NoisedYVectorGenerator;
  typedef typename Simulator::AnalyticXCovarianceCalculator
    AnalyticXCovarianceCalculator;
  typedef typename Simulator::StatisticalXCovarianceCalculator
    StatisticalXCovarianceCalculator;
  typedef typename Simulator::XVectorOptimizor
    XVectorOptimizor;
  typedef typename Simulator::ResidualsCalculator
    ResidualsCalculator;
  typedef typename Simulator::MahalanobisDistanceCalculator
    MahalanobisDistanceCalculator;
  typedef typename Simulator::XVector XVector;
  typedef typename Simulator::YVector YVector;
  typedef typename Simulator::YCovarianceInverse YCovarianceInverse;
  typedef typename Simulator::XCovariance XCovariance;

  typedef hs::sfm::ba::BANaiveNoisedXGenerator<Scalar>
    NoisedXVecotrGenerator;

  typedef EIGEN_MATRIX(Scalar, 2, 2) Matrix22;
  typedef EIGEN_MATRIX(Scalar, 3, 3) Matrix33;
  typedef EIGEN_MATRIX(Scalar, Eigen::Dynamic, Eigen::Dynamic) MatrixXX;
  typedef EIGEN_VECTOR(Scalar, 3) Vector3;

  static Err Test(const VectorFunction& vector_function,
                  const XVector& true_x,
                  const Scalar& feature_stddev,
                  Scalar x_rotation_stddev,
                  Scalar y_rotation_stddev,
                  Scalar z_rotation_stddev,
                  Scalar x_pos_stddev,
                  Scalar y_pos_stddev,
                  Scalar z_pos_stddev,
                  Scalar x_point_stddev,
                  Scalar y_point_stddev,
                  Scalar z_point_stddev,
                  size_t simulator_size)
  {
    YVector true_y;
    if (vector_function(true_x, true_y) != 0)
    {
      std::cout<<"vector function failed!\n";
      return -1;
    }

    XVector near_x;
    NoisedXVecotrGenerator noised_x_generator;
    if (noised_x_generator(vector_function,
                           true_x,
                           x_rotation_stddev,
                           y_rotation_stddev,
                           z_rotation_stddev,
                           x_pos_stddev,
                           y_pos_stddev,
                           z_pos_stddev,
                           x_point_stddev,
                           y_point_stddev,
                           z_point_stddev,
                           near_x) != 0)
    {
      std::cout<<"noised x vector generator failed!\n";
      return -1;
    }

    XVectorOptimizor x_optimizor(near_x, 100,
                                 Scalar(1e-6),
                                 Scalar(1e-10),
                                 Scalar(1e-10));

    NoisedYVectorGenerator noised_y_generator;

    AnalyticXCovarianceCalculator analytic_x_covariance_calculator;

    ResidualsCalculator residuals_calculator;

    MahalanobisDistanceCalculator mahalanobis_distance_calculator;

    Index number_of_feature = vector_function.number_of_features();
    YCovarianceInverse y_covariance_inverse;
    Matrix22 feature_covariance = Matrix22::Zero();
    feature_covariance(0, 0) = feature_stddev * feature_stddev;
    feature_covariance(1, 1) = feature_stddev * feature_stddev;
    if (GenerateFeatureCovarianceInverse(feature_covariance, number_of_feature,
                      y_covariance_inverse) != 0)
    {
      std::cout<<"GenFeatCovInv Failed.\n";
      return -1;
    }

    StatisticalXCovarianceCalculator statistical_x_covariance_calculator;
    XCovariance analytic_x_covariance;
    XCovariance statistical_x_covariance;
    Scalar residual_mean;

    Simulator simulator;
    if (simulator(vector_function,
                  x_optimizor,
                  residuals_calculator,
                  mahalanobis_distance_calculator,
                  noised_y_generator,
                  analytic_x_covariance_calculator,
                  true_y,
                  y_covariance_inverse,
                  true_x,
                  simulator_size,
                  statistical_x_covariance_calculator,
                  residual_mean,
                  analytic_x_covariance,
                  statistical_x_covariance) != 0)
    {
      std::cout<<"simulator failed!\n";
      return -1;
    }
    residual_mean = std::sqrt(residual_mean / Scalar(true_y.rows()));

    //计算本质参数数
    JacobianMatrixCalculator jacobian;
    JacobianMatrix jacobian_matrix;
    if (jacobian(vector_function, true_x, jacobian_matrix) != 0)
    {
      std::cout<<"jacobian failed!\n";
      return -1;
    }
    Index x_size = vector_function.GetXSize();
    Index y_size = vector_function.GetYSize();
    MatrixXX dense_jacobian_matrix(y_size, x_size);
    for (Index i = 0; i < y_size; i++)
    {
      for (Index j = 0; j < x_size; j++)
      {
        dense_jacobian_matrix(i, j) = jacobian_matrix.coeff(i, j);
      }
    }
    Eigen::FullPivLU<MatrixXX> lu(dense_jacobian_matrix);
    Index rank = lu.rank();

    Scalar expected_residual_mean =
      std::sqrt(Scalar(1) - Scalar(rank) / Scalar(y_size));

    Err result = 0;
    if (std::abs(residual_mean - expected_residual_mean) > 5e-2)
    {
      std::cout<<"residual mean too large\n";
      std::cout<<"expected residual mean:"<<expected_residual_mean<<"\n";
      std::cout<<"residual mean:"<<residual_mean<<"\n";
      result = -1;
    }

    //const Scalar threshold = Scalar(1e-2);
    //for (Index i = 0; i < analytic_x_covariance.rows(); i++)
    //{
    //  for (Index j = 0; j < analytic_x_covariance.cols(); j++)
    //  {
    //    Scalar analytic_value = analytic_x_covariance(i, j);
    //    Scalar statistical_value = statistical_x_covariance(i, j);
    //    Scalar error = std::abs(analytic_value - statistical_value);
    //    if (error > threshold)
    //    {
    //      std::cout<<"difference between analytic value statistical value "
    //                 "is to large!\n";
    //      std::cout<<"analytic_x_covariance["<<i<<", "<<j<<"]="
    //               <<analytic_value<<" \n";
    //      std::cout<<"statistical_x_covariance["<<i<<", "<<j<<"]="
    //               <<statistical_value<<" \n";
    //      result = -1;
    //    }
    //  }
    //}

    return result;
  }

private:
  static Err GenerateFeatureCovarianceInverse(
    const Matrix22& feature_covariance,
    Index number_of_feature,
    YCovarianceInverse& y_covariance_inverse)
  {
    y_covariance_inverse.blocks.clear();
    for (Index i = 0; i < number_of_feature; i++)
    {
      y_covariance_inverse.blocks.push_back(feature_covariance.inverse());
    }

    return 0;
  }

};

TEST(TestBANaiveMonteCarloSimulate, SmallDataTest)
{
  typedef double Scalar;
  typedef size_t ImageDimension;

  typedef hs::sfm::ba::BANaiveSyntheticDataGenerator<Scalar, ImageDimension>
          DataGenerator;
  typedef TestBANaiveMonteCarloSimulate<Scalar> Test;
  typedef Test::Matrix22 Matrix22;
  typedef Test::VectorFunction VectorFunction;
  typedef Test::XVector XVector;
  typedef Test::YVector YVector;

  Scalar focal_length_in_metre = 0.019;
  size_t number_of_strips = 3;
  size_t number_of_cameras_in_strip = 3;
  Scalar ground_resolution = 0.1;
  ImageDimension image_width = 6000;
  ImageDimension image_height = 4000;
  Scalar pixel_size = 0.0000039;
  size_t number_of_points = 50;
  Scalar lateral_overlap_ratio = 0.6;
  Scalar longitudinal_overlap_ratio = 0.8;
  Scalar scene_max_height = 50;
  Scalar camera_height_stddev = 5;
  Scalar camera_planar_stddev = 5;
  Scalar camera_rotation_stddev = 1;
  Scalar north_west_angle = 60;

  VectorFunction vector_function;
  XVector x;
  YVector y;

  DataGenerator data_generator(focal_length_in_metre,
                               number_of_strips,
                               number_of_cameras_in_strip,
                               ground_resolution,
                               image_width,
                               image_height,
                               pixel_size,
                               number_of_points,
                               lateral_overlap_ratio,
                               longitudinal_overlap_ratio,
                               scene_max_height,
                               camera_height_stddev,
                               camera_planar_stddev,
                               camera_rotation_stddev,
                               north_west_angle);
  ASSERT_EQ(0, data_generator(vector_function, x, y));
  Scalar focal_length_in_pixel = focal_length_in_metre / pixel_size;

  ASSERT_EQ(0, Test::Test(vector_function, x, 
                          Scalar(1) / focal_length_in_pixel,
                          1.0, 1.0, 1.0, 1.0, 1.0, 1.0,
                          1.0, 1.0, 1.0,
                          100));
}

}
