#pragma once

#include <gtsam/nonlinear/NonlinearFactor.h>
#include <gtsam/geometry/PinholeCamera.h>
#include <gtsam/geometry/Cal3_S2.h>
#include <boost/optional.hpp>

namespace gtsam {

  /**
   * Non-linear factor for a constraint derived from a 2D measurement. The calibration is known here.
   * i.e. the main building block for visual SLAM.
   * @addtogroup SLAM
   */
  template<class POSE, class LANDMARK, class CALIBRATION = Cal3_S2>
  class ProjectionFactorCalibrationFull: public NoiseModelFactor3<POSE, POSE, LANDMARK> {
  protected:

    // Keep a copy of measurement and calibration for I/O
    Point2 measured_;                    ///< 2D measurement
    boost::shared_ptr<CALIBRATION> K_;  ///< shared pointer to calibration object

    // verbosity handling for Cheirality Exceptions
    bool throwCheirality_; ///< If true, rethrows Cheirality exceptions (default: false)
    bool verboseCheirality_; ///< If true, prints text for Cheirality exceptions (default: false)

  public:

    /// shorthand for base class type
    typedef NoiseModelFactor3<POSE, POSE, LANDMARK> Base;

    /// shorthand for this class
    typedef ProjectionFactorCalibrationFull<POSE, LANDMARK, CALIBRATION> This;

    /// shorthand for a smart pointer to a factor
    typedef boost::shared_ptr<This> shared_ptr;

    /// Default constructor
  ProjectionFactorCalibrationFull() :
      measured_(0.0, 0.0), throwCheirality_(false), verboseCheirality_(false) {
  }

    /**
     * Constructor
     * TODO: Mark argument order standard (keys, measurement, parameters)
     * @param measured is the 2 dimensional location of point in image (the measurement)
     * @param model is the standard deviation
     * @param poseKey is the index of the camera
     * @param transformKey is the index of the body-camera transform
     * @param pointKey is the index of the landmark
     * @param K shared pointer to the constant calibration
     */
    ProjectionFactorCalibrationFull(const Point2& measured, const SharedNoiseModel& model,
        Key poseKey, Key transformKey,  Key pointKey,
        const boost::shared_ptr<CALIBRATION>& K) :
          Base(model, poseKey, transformKey, pointKey), measured_(measured), K_(K),
          throwCheirality_(false), verboseCheirality_(false) {}

    /**
     * Constructor with exception-handling flags
     * TODO: Mark argument order standard (keys, measurement, parameters)
     * @param measured is the 2 dimensional location of point in image (the measurement)
     * @param model is the standard deviation
     * @param poseKey is the index of the camera
     * @param pointKey is the index of the landmark
     * @param K shared pointer to the constant calibration
     * @param throwCheirality determines whether Cheirality exceptions are rethrown
     * @param verboseCheirality determines whether exceptions are printed for Cheirality
     */
    ProjectionFactorCalibrationFull(const Point2& measured, const SharedNoiseModel& model,
        Key poseKey, Key transformKey, Key pointKey,
        const boost::shared_ptr<CALIBRATION>& K,
        bool throwCheirality, bool verboseCheirality) :
          Base(model, poseKey, transformKey, pointKey), measured_(measured), K_(K),
          throwCheirality_(throwCheirality), verboseCheirality_(verboseCheirality) {}

    /** Virtual destructor */
    virtual ~ProjectionFactorCalibrationFull() {}

    /// @return a deep copy of this factor
    virtual NonlinearFactor::shared_ptr clone() const {
      return boost::static_pointer_cast<NonlinearFactor>(
          NonlinearFactor::shared_ptr(new This(*this))); }

    /**
     * print
     * @param s optional string naming the factor
     * @param keyFormatter optional formatter useful for printing Symbols
     */
    void print(const std::string& s = "", const KeyFormatter& keyFormatter = DefaultKeyFormatter) const {
      std::cout << s << "ProjectionFactorCalibrationFull, z = ";
      traits<Point2>::Print(measured_);
      Base::print("", keyFormatter);
    }

    /// equals
    virtual bool equals(const NonlinearFactor& p, double tol = 1e-9) const {
      const This *e = dynamic_cast<const This*>(&p);
      return e
          && Base::equals(p, tol)
          && traits<Point2>::Equals(this->measured_, e->measured_, tol)
          && this->K_->equals(*e->K_, tol);
    }

    /// Evaluate error h(x)-z and optionally derivatives
    Vector evaluateError(const Pose3& pose, const Pose3& transform, const Point3& point,
        boost::optional<Matrix&> H1 = boost::none,
        boost::optional<Matrix&> H2 = boost::none,
        boost::optional<Matrix&> H3 = boost::none) const {
      try {
          if(H1 || H2 || H3) {
            Matrix H0, H02;
            PinholeCamera<CALIBRATION> camera(pose.compose(transform, H0, H02), *K_);
            Point2 reprojectionError(camera.project(point, H1, H3, boost::none) - measured_);
            *H2 = *H1 * H02;
            *H1 = *H1 * H0;
            return reprojectionError;
          } else {
            PinholeCamera<CALIBRATION> camera(pose.compose(transform), *K_);
            return camera.project(point, H1, H3, boost::none) - measured_;
          }
      } catch( CheiralityException& e) {
        if (H1) *H1 = Matrix::Zero(2,6);
        if (H2) *H2 = Matrix::Zero(2,6);
        if (H3) *H3 = Matrix::Zero(2,3);
        if (verboseCheirality_)
          std::cout << e.what() << ": Landmark "<< DefaultKeyFormatter(this->key2()) <<
              " moved behind camera " << DefaultKeyFormatter(this->key1()) << std::endl;
        if (throwCheirality_)
          throw e;
      }
      return Vector::Ones(2) * 2.0 * K_->fx();
    }

    /** return the measurement */
    const Point2& measured() const {
      return measured_;
    }

    /** return the calibration object */
    inline const boost::shared_ptr<CALIBRATION> calibration() const {
      return K_;
    }

    /** return verbosity */
    inline bool verboseCheirality() const { return verboseCheirality_; }

    /** return flag for throwing cheirality exceptions */
    inline bool throwCheirality() const { return throwCheirality_; }

  private:

    /// Serialization function
    friend class boost::serialization::access;
    template<class ARCHIVE>
    void serialize(ARCHIVE & ar, const unsigned int /*version*/) {
      ar & BOOST_SERIALIZATION_BASE_OBJECT_NVP(Base);
      ar & BOOST_SERIALIZATION_NVP(measured_);
      ar & BOOST_SERIALIZATION_NVP(K_);
      ar & BOOST_SERIALIZATION_NVP(throwCheirality_);
      ar & BOOST_SERIALIZATION_NVP(verboseCheirality_);
    }
  };

  /// traits
  template<class POSE, class LANDMARK, class CALIBRATION>
  struct traits<ProjectionFactorCalibrationFull<POSE, LANDMARK, CALIBRATION> > :
      public Testable<ProjectionFactorCalibrationFull<POSE, LANDMARK, CALIBRATION> > {
  };

} // \ namespace gtsam
