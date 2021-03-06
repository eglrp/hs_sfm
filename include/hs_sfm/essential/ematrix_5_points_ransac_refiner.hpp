﻿#ifndef _HS_SFM_ESSENTIAL_EMATRIX_5_POINTS_RANSAC_REFINER_HPP_
#define _HS_SFM_ESSENTIAL_EMATRIX_5_POINTS_RANSAC_REFINER_HPP_

#include <utility>

#include "hs_fit/ransac/ransac.hpp"

#include "hs_sfm/essential/ematrix_5_points_calculator.hpp"

namespace hs
{
namespace fit
{

template <typename _Scalar>
struct PointSetType<std::pair<EIGEN_VECTOR(_Scalar, 3),
                              EIGEN_VECTOR(_Scalar, 3)> >
{
  typedef std::pair<EIGEN_VECTOR(_Scalar, 3),
                    EIGEN_VECTOR(_Scalar, 3)> Point;
  typedef EIGEN_STD_VECTOR(Point) type;
};

}

namespace sfm
{
namespace essential
{

template <typename _Scalar>
class EMatrix5PointsRansacRefiner	//!本质矩阵5点方法的ransac优化类
{
public:
  typedef _Scalar Scalar;
  typedef int Err;
  
private:
  typedef EMatrix5PointsCalculator<Scalar> Calculator;
public:
  typedef typename Calculator::HKey HKey;	//3*3矩阵
  typedef typename Calculator::HKeyPair HKeyPair;	//
  typedef typename Calculator::HKeyPairContainer HKeyPairContainer;
  typedef typename Calculator::EMatrix EMatrix;
private:
  typedef typename Calculator::EMatrixHypotheses EMatrixHypotheses;	//EMatrixHypotheses的类型为EMatrix的向量

  class RansacModelCalculator	//Ransac类的模板参数之一
  {
  public:
    typedef EMatrixHypotheses RansacModel;	//ransac模型, 本质矩阵估算器, 见EMatrix5PointsCalculator<Scalar>
    static const size_t model_min_set_size_ = 5;

  private:
    typedef typename Calculator::HypothesesGenerator Generator;

  public:
    int operator() (const HKeyPairContainer& key_pairs,
                    EMatrixHypotheses& ematrix_hypotheses) const
    {
      Generator generator;
      return generator(key_pairs, ematrix_hypotheses);

      return 0;
    }
  };

  class RansacDistanceCalculator
  {
  public:
    typedef Scalar Distance;

  private:
    typedef typename Calculator::EMatrixEvaluator Evaluator;

  public:
    int operator() (const HKeyPair& key_pair,
                    const EMatrixHypotheses& ematrix_hypotheses,
                    Distance& distance) const
    {
      Evaluator evaluator;
      distance = std::numeric_limits<Scalar>::max();
      for (size_t i = 0; i < ematrix_hypotheses.size(); i++)
      {
        Scalar distance_hypotheses;
        evaluator(key_pair, ematrix_hypotheses[i], distance_hypotheses);
        if (distance_hypotheses < distance)
        {
          distance = distance_hypotheses;
        }
      }

      return 0;
    }
  };

  struct RansacInlierEvaluator
  {
    typedef typename HKeyPairContainer::difference_type Index;
    typedef typename Calculator::EMatrixEvaluator Evaluator;
    typedef std::vector<Index> IndexSet;
    void operator() (const HKeyPairContainer& key_pairs,
                     const EMatrixHypotheses& ematrix_hypotheses,
                     RansacDistanceCalculator& distance_calculator,
                     Scalar distance_threshold,
                     std::vector<Scalar>& inlier_distances,
                     IndexSet& inlier_indices,
                     HKeyPairContainer& inliers) const
    {
      Evaluator evaluator;
      inlier_distances.clear();
      inlier_indices.clear();
      inliers.clear();
      Scalar threshold_sqr = distance_threshold * distance_threshold;
      Scalar min_score = std::numeric_limits<Scalar>::max();
      for (size_t i = 0; i < ematrix_hypotheses.size(); i++)
      {
        std::vector<Scalar> inlier_distances_hypotheses;
        IndexSet inlier_indices_hypotheses;
        HKeyPairContainer inliers_hypotheses;
        Scalar score = Scalar(0);
        for (size_t j = 0; j < key_pairs.size(); j++)
        {
          Scalar distance;
          evaluator(key_pairs[j], ematrix_hypotheses[i], distance);
          score += std::log(Scalar(1) +
            distance * distance / threshold_sqr);
          if (distance < distance_threshold)
          {
            inlier_distances_hypotheses.push_back(distance);
            inliers_hypotheses.push_back(key_pairs[j]);
            inlier_indices_hypotheses.push_back(j);
          }
        }
        if (inliers_hypotheses.size() > inliers.size() ||
            (inliers_hypotheses.size() == inliers.size() &&
             score < min_score))
        {
          inlier_distances.swap(inlier_distances_hypotheses);
          inlier_indices.swap(inlier_indices_hypotheses);
          inliers.swap(inliers_hypotheses);
          min_score = score;
        }
      }
    }
  };

  typedef hs::fit::Ransac<HKeyPair,
                          RansacModelCalculator,
                          RansacDistanceCalculator,
                          RansacInlierEvaluator> RansacRefiner;

public:
  typedef typename RansacRefiner::IndexSet IndexSet;

public:
  Err operator() (const HKeyPairContainer& key_pairs,
                  Scalar distance_threshold,
                  HKeyPairContainer& refined_key_pairs,	//输出参数
                  IndexSet& inlier_indices,
                  EMatrix& ematrix) const	//
  {
    typedef typename Calculator::EMatrixEvaluator Evaluator;	//ransac求值后的评估, 来自EMatrix5PointsCalculator
    RansacModelCalculator model_calculator;
    RansacDistanceCalculator distance_calculator;
    RansacRefiner ransac_refiner(model_calculator, distance_calculator);	//先配置Ransac

    ransac_refiner.SetAlphaThreshold(Scalar(0.95));
    ransac_refiner.SetDistanceThreshold(distance_threshold);
    HKeyPairContainer best_key_pairs;
    EMatrixHypotheses ematrix_hypotheses;
    Err result = ransac_refiner(key_pairs, refined_key_pairs,
                                inlier_indices, best_key_pairs,
                                &ematrix_hypotheses);	//调用Ransac, 

    if (result != 0) return result;	//若返回结果非零, 则表示求解未正常结束.
    Evaluator evaluator;
    Scalar threshold_sqr = distance_threshold * distance_threshold;
    Scalar min_score = std::numeric_limits<Scalar>::max();
    size_t max_number_of_inliers = 0;
    for (size_t i = 0; i < ematrix_hypotheses.size(); i++)	//遍历求解的E矩阵估计向量, 评估其结果.
    {
      Scalar score = Scalar(0);
      size_t number_of_inliers = 0;
      for (size_t j = 0; j < key_pairs.size(); j++)
      {
        Scalar distance;
        evaluator(key_pairs[j], ematrix_hypotheses[i], distance);	//计算点与对极线的距离.作为误差评定标准.
        score += std::log(Scalar(1) +
          distance * distance / threshold_sqr);
        if (distance < distance_threshold)
        {
          number_of_inliers++;
        }
      }
      if (number_of_inliers > max_number_of_inliers ||
          (number_of_inliers == max_number_of_inliers &&
           score < min_score))
      {
        max_number_of_inliers = number_of_inliers;
        min_score = score;
        ematrix = ematrix_hypotheses[i];
      }
    }

    return result;
  }

};

}
}
}

#endif
