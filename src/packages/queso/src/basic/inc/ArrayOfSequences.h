//-----------------------------------------------------------------------bl-
//--------------------------------------------------------------------------
//
// QUESO - a library to support the Quantification of Uncertainty
// for Estimation, Simulation and Optimization
//
// Copyright (C) 2008-2015 The PECOS Development Team
//
// This library is free software; you can redistribute it and/or
// modify it under the terms of the Version 2.1 GNU Lesser General
// Public License as published by the Free Software Foundation.
//
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
// Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public
// License along with this library; if not, write to the Free Software
// Foundation, Inc. 51 Franklin Street, Fifth Floor,
// Boston, MA  02110-1301  USA
//
//-----------------------------------------------------------------------el-

#ifndef UQ_ARRAY_OF_SEQUENCES_H
#define UQ_ARRAY_OF_SEQUENCES_H

#include <queso/VectorSequence.h>

namespace QUESO {

/*! \file uqArrayOfSequences.h
 * \brief A templated class for handling samples of scalar sequences
 *
 * \class ArrayOfSequences
 * \brief Class for handling array samples (arrays of scalar sequences).
 *
 * This class handles array of sequences (samples) generated by an algorithm,
 * as well as operations that can be carried over them, e.g., calculation of
 * means, correlation and covariance matrices. It is derived from and implements
 * BaseVectorSequence<V,M>.*/

template <class V, class M>
class ArrayOfSequences : public BaseVectorSequence<V,M>
{
public:
  //! @name Constructor/Destructor methods
  //@{
  //! Default constructor.
  ArrayOfSequences(const VectorSpace<V,M>& vectorSpace,
                          unsigned int                   subSequenceSize,
                          const std::string&             name);
  //! Destructor.
  ~ArrayOfSequences();
  //@}

  //! @name Sequence methods
  //@{
  //! Size of the sub-sequence of arrays.
  unsigned int subSequenceSize      () const;

  //! Resizes the sequence.
  void         resizeSequence       (unsigned int newSubSequenceSize);

  //! Resets a total of \c numPos values of the sequence starting at position  \c initialPos.
  /*! This routine deletes all stored computed vectors */
  void         resetValues          (unsigned int initialPos, unsigned int numPos);

  //! Erases \c numPos elements of the sequence starting at position  \c initialPos.
  /*! This routine deletes all stored computed vectors */
  void         erasePositions       (unsigned int initialPos, unsigned int numPos);

  //! Gets the values of the sequence at position \c posId and stores them at \c vec.
  void         getPositionValues    (unsigned int posId,       V& vec) const;

  //! Set the values of \c vec at position \c posId  of the sequence.
  /*! This routine deletes all stored computed vectors */
  void         setPositionValues    (unsigned int posId, const V& vec);

  //! Sets the values of the sequence as a Gaussian distribution of mean given by \c meanVec and standard deviation by \c stdDevVec.
  /*! This routine deletes all stored computed vectors */
  void         setGaussian          (const V& meanVec, const V& stdDevVec);

  //! Sets the values of the sequence as a uniform distribution between the values given by vectors \c aVec and \c bVec.
  /*! This routine deletes all stored computed vectors */
  void         setUniform           (const V& aVec,    const V& bVec     );

  //! Finds the mean value of the sub-sequence, considering \c numPos positions starting at position \c initialPos.
  void         mean                 (unsigned int             initialPos,
                                     unsigned int             numPos,
                                     V&                       meanVec) const;
  //! Finds the mean value of the unified sequence of \c numPos positions starting at position \c initialPos. See template specialization.
  // TODO not implemented.
  void         unifiedMean          (unsigned int             initialPos,
                                     unsigned int             numPos,
                                     V&                       unifiedMeanVec) const;
  //! Finds the sample variance of the sub-sequence, considering \c numPos positions starting at position \c initialPos and of mean \c meanVec.
  void         sampleVariance       (unsigned int             initialPos,
                                     unsigned int             numPos,
                                     const V&                 meanVec,
                                     V&                       samVec) const;
  //! Finds the sample variance of the unified sequence,  considering \c numPos positions starting at position \c initialPos and of mean \c meanVec.
  // TODO not implemented.
  void         unifiedSampleVariance(unsigned int             initialPos,
                                     unsigned int             numPos,
                                     const V&                 meanVec,
                                     V&                       samVec) const;
  //! Finds the population variance of the sub-sequence,  considering \c numPos positions starting at position \c initialPos and of mean \c meanVec.
  void         populationVariance   (unsigned int             initialPos,
                                     unsigned int             numPos,
                                     const V&                 meanVec,
                                     V&                       popVec) const;
  //! Calculates the autocovariance.
  /*! The autocovariance is the covariance of a variable with itself at some other time. It is
   * calculated over a sequence of arrays with initial position \c initialPos, considering
   * \c numPos positions, a lag of \c lag, with mean given by \c meanVec. The results are saved
   * in the output vector \c covVec/   */
  void         autoCovariance       (unsigned int             initialPos,
                                     unsigned int             numPos,
                                     const V&                 meanVec,
                                     unsigned int             lag,
                                     V&                       covVec) const;

  //! Calculates autocorrelation via definition.
  void         autoCorrViaDef       (unsigned int             initialPos,
                                     unsigned int             numPos,
                                     unsigned int             lag,
                                     V&                       corrVec) const;
  //! Calculates autocorrelation via Fast Fourier transforms (FFT).
  //! TODO: Implement me!
  void         autoCorrViaFft       (unsigned int                     initialPos,
                                     unsigned int                     numPos,
                                     const std::vector<unsigned int>& lags,
                                     std::vector<V*>&                 corrVecs) const;

  //! Calculates autocorrelation via Fast Fourier transforms (FFT).
  //! TODO: Implement me!
  void         autoCorrViaFft       (unsigned int             initialPos,
                                     unsigned int             numPos,
                                     unsigned int             numSum,
                                     V&                       autoCorrsSumVec) const;
   //! Given an initial position \c initialPos, finds the minimum and the maximum values of the sequence.
  void         minMax               (unsigned int             initialPos,
                                     V&                       minVec,
                                     V&                       maxVec) const;
  //! Calculates the histogram of the sequence.
  /*! TODO: implement me! */
  void         histogram            (unsigned int             initialPos,
                                     const V&                 minVec,
                                     const V&                 maxVec,
                                     std::vector<V*>&         centersForAllBins,
                                     std::vector<V*>&         quanttsForAllBins) const;

  //! Returns the interquartile range of the values in the sequence.
  /*! TODO: implement me! */
  void         interQuantileRange   (unsigned int             initialPos,
                                     V&                       iqrs) const;

  //! Selects the scales (bandwidth, \c scaleVec) for the kernel density estimation, of  the sequence.
  /*! TODO: implement me! */
  void         scalesForKDE         (unsigned int             initialPos,
                                     const V&                 iqrs,
                                     unsigned int             kdeDimension,
                                     V&                       scales) const;
  //! Gaussian kernel for the KDE estimate of the sequence.
  /*! TODO: implement me! */
  void         gaussianKDE          (const V&                 evaluationParamVec,
                                     V&                       densityVec) const;
  //! TODO: Gaussian kernel for the KDE of the sequence.
  /*! \todo: implement me!
   * The density estimator will be:
   * \f[ \hat{f}(x) = \frac{1}{nh} \sum_{j=1}^n K\Big(\frac{x-x_j}{h}\Big),\f]
   * where \f$ K \f$ is a Gaussian kernel.  */
  void         gaussianKDE          (unsigned int             initialPos,
                                     const V&                 scales,
                                     const std::vector<V*>&   evaluationParamVecs,
                                     std::vector<V*>&         densityVecs) const;
  //! Write contents of the chain to a file.
  void         writeContents        (std::ofstream&                   ofsvar) const;

  //! Writes info of the unified sequence to a file.
  /*! TODO: implement me! */
  void         unifiedWriteContents (std::ofstream&                   ofsvar) const;

  //! Writes info of the unified sequence to a file.
  /*! TODO: implement me! */
  void         unifiedWriteContents (const std::string&               fileName,
                                     const std::string&               fileType) const;

  //! Reads the unified sequence from a file.
  /*! TODO: implement me! */
  void         unifiedReadContents  (const std::string&               fileName,
                                     const std::string&               fileType,
                                     const unsigned int               subSequenceSize);

  //! Select positions in the sequence of vectors.
  /*! TODO: implement me! */
  void         select               (const std::vector<unsigned int>& idsOfUniquePositions);

  //! Filters positions in the sequence of vectors, starting at \c initialPos, and with spacing given by \c spacing.
  /*! TODO: implement me! */
  void         filter               (unsigned int                     initialPos,
                                     unsigned int                     spacing);

  //! Extracts a sequence of scalars of size \c numPos, from position \c paramId of the array of sequences.
  /*! In this method, the parameter \c initialPos is unused and the parameter \c spacing  is always equal
   * to one */
  void         extractScalarSeq     (unsigned int                   initialPos,
                                     unsigned int                   spacing,
                                     unsigned int                   numPos,
                                     unsigned int                   paramId,
                                     ScalarSequence<double>& scalarSeq) const;

#ifdef UQ_ALSO_COMPUTE_MDFS_WITHOUT_KDE
  void         uniformlySampledMdf  (const V&                       numEvaluationPointsVec,
                                     ArrayOfOneDGrids <V,M>& mdfGrids,
                                     ArrayOfOneDTables<V,M>& mdfValues) const;
#endif

#ifdef QUESO_COMPUTES_EXTRA_POST_PROCESSING_STATISTICS
  void         uniformlySampledCdf  (const V&                       numEvaluationPointsVec,
                                     ArrayOfOneDGrids <V,M>& cdfGrids,
                                     ArrayOfOneDTables<V,M>& cdfValues) const;
  void         bmm                  (unsigned int             initialPos,
                                     unsigned int             batchLength,
                                     V&                       bmmVec) const;
  void         fftForward           (unsigned int                        initialPos,
                                     unsigned int                        fftSize,
                                     unsigned int                        paramId,
                                     std::vector<std::complex<double> >& resultData) const;
//void         fftInverse           (unsigned int fftSize);

  void         psd                  (unsigned int             initialPos,
                                     unsigned int             numBlocks,
                                     double                   hopSizeRatio,
                                     unsigned int             paramId,
                                     std::vector<double>&     psdResult) const;
  void         psdAtZero            (unsigned int             initialPos,
                                     unsigned int             numBlocks,
                                     double                   hopSizeRatio,
                                     V&                       psdVec) const;
  void         geweke               (unsigned int             initialPos,
                                     double                   ratioNa,
                                     double                   ratioNb,
                                     V&                       gewVec) const;
#endif
 //@}

private:
  //! Extracts the raw data.
  /*! Extracts the data from sequence of array at position \c paramId, with spacing \c spacing,
   * and saves is in \c rawData. The vector \c rawData will have size \c numPos. Note that, in
   * fact, \c spacing = 1. */
  void         extractRawData       (unsigned int                   initialPos,
                                     unsigned int                   spacing,
                                     unsigned int                   numPos,
                                     unsigned int                   paramId,
                                     std::vector<double>&           rawData) const;

  //! Sequence of scalars
  DistArray<ScalarSequence<double>*> m_scalarSequences;

  using BaseVectorSequence<V,M>::m_env;
  using BaseVectorSequence<V,M>::m_vectorSpace;
  using BaseVectorSequence<V,M>::m_name;
  using BaseVectorSequence<V,M>::m_fftObj;
};

}  // End namespace QUESO

#endif // UQ_ARRAY_OF_SEQUENCES_H
