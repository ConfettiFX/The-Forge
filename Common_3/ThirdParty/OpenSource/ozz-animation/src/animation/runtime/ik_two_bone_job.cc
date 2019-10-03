//----------------------------------------------------------------------------//
//                                                                            //
// ozz-animation is hosted at http://github.com/guillaumeblanc/ozz-animation  //
// and distributed under the MIT License (MIT).                               //
//                                                                            //
// Copyright (c) 2019 Guillaume Blanc                                         //
//                                                                            //
// Permission is hereby granted, free of charge, to any person obtaining a    //
// copy of this software and associated documentation files (the "Software"), //
// to deal in the Software without restriction, including without limitation  //
// the rights to use, copy, modify, merge, publish, distribute, sublicense,   //
// and/or sell copies of the Software, and to permit persons to whom the      //
// Software is furnished to do so, subject to the following conditions:       //
//                                                                            //
// The above copyright notice and this permission notice shall be included in //
// all copies or substantial portions of the Software.                        //
//                                                                            //
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR //
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,   //
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL    //
// THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER //
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING    //
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER        //
// DEALINGS IN THE SOFTWARE.                                                  //
//                                                                            //
//----------------------------------------------------------------------------//

#include "../../../include/ozz/animation/runtime/ik_two_bone_job.h"

#include "../../../../../Common_3/OS/Interfaces/ILog.h"

namespace ozz {
namespace animation {
IKTwoBoneJob::IKTwoBoneJob()
    : target(0.0f),
      mid_axis(Vector3::zAxis()),
      pole_vector(Vector3::yAxis()),
      twist_angle(0.f),
      soften(1.f),
      weight(1.f),
      start_joint(NULL),
      mid_joint(NULL),
      end_joint(NULL),
      start_joint_correction(NULL),
      mid_joint_correction(NULL),
      reached(NULL) {}

bool IKTwoBoneJob::Validate() const {
  bool valid = true;
  valid &= start_joint && mid_joint && end_joint;
  valid &= start_joint_correction && mid_joint_correction;
  valid &= isNormalizedEst(mid_axis);
  return valid;
}

namespace {

// Local data structure used to share constant data accross ik stages.
struct IKConstantSetup {
  IKConstantSetup(const IKTwoBoneJob& _job) {
    // Prepares constants
    one = Vector4::one();
    mask_sign = vector4int::mask_sign();
    m_one = xorPerElem(one, mask_sign);

    // Computes inverse matrices required to change to start and mid spaces.
    // If matrices aren't invertible, they'll be all 0 (ozz::math
    // implementation), which will result in identity correction quaternions.
    inv_start_joint = inverse(*_job.start_joint);
    const Matrix4 inv_mid_joint = inverse(*_job.mid_joint);

    // Transform some positions to mid joint space (_ms)
    const Vector3 start_ms =
        (inv_mid_joint * Point3(_job.start_joint->getCol3().get128())).getXYZ();
    const Vector3 end_ms =
        (inv_mid_joint * Point3(_job.end_joint->getCol3().get128())).getXYZ();

    // Transform some positions to start joint space (_ss)
    const Vector3 mid_ss =
        (inv_start_joint * Point3(_job.mid_joint->getCol3().get128())).getXYZ();
    const Vector3 end_ss =
        (inv_start_joint * Point3(_job.end_joint->getCol3().get128())).getXYZ();

    // Computes bones vectors and length in mid and start spaces.
    // Start joint position will be treated as 0 because all joints are
    // expressed in start joint space.
    start_mid_ms = -start_ms;
    mid_end_ms = end_ms;
    start_mid_ss = mid_ss;
    const Vector3 mid_end_ss = end_ss - mid_ss;
    const Vector3 start_end_ss = end_ss;
    start_mid_ss_len2 = lengthSqr(start_mid_ss);
    mid_end_ss_len2 = lengthSqr(mid_end_ss);
    start_end_ss_len2 = lengthSqr(start_end_ss);
  }

  // Constants
  Vector4 one;
  Vector4 m_one;
  Vector4Int mask_sign;

  // Inverse matrices
  Matrix4 inv_start_joint;

  // Bones vectors and length in mid and start spaces (_ms and _ss).
  Vector3 start_mid_ms;
  Vector3 mid_end_ms;
  Vector3 start_mid_ss;
  FloatInVec start_mid_ss_len2;
  FloatInVec mid_end_ss_len2;
  FloatInVec start_end_ss_len2;
};

// Smoothen target position when it's further that a ratio of the joint chain
// length, and start to target length isn't 0.
// Inspired by http://www.softimageblog.com/archives/108
// and http://www.ryanjuckett.com/programming/analytic-two-bone-ik-in-2d/
bool SoftenTarget(const IKTwoBoneJob& _job, const IKConstantSetup& _setup,
                  Vector3* _start_target_ss,
                  FloatInVec* _start_target_ss_len2) {
  // Hanlde position in start joint space (_ss)
  const Vector3 start_target_original_ss =
      (_setup.inv_start_joint * _job.target).getXYZ();
  const FloatInVec start_target_original_ss_len2 =
      lengthSqr(start_target_original_ss);
  const Vector3 lengths =
      sqrtPerElem(Vector3(_setup.start_mid_ss_len2, _setup.mid_end_ss_len2,
                          start_target_original_ss_len2));
  const FloatInVec start_mid_ss_len = lengths.getX();
  const FloatInVec mid_end_ss_len = lengths.getY();
  const FloatInVec start_target_original_ss_len = lengths.getZ();
  const FloatInVec bone_len_diff_abs =
      andNotPerElem(start_mid_ss_len - mid_end_ss_len, _setup.mask_sign);
  const FloatInVec bones_chain_len = start_mid_ss_len + mid_end_ss_len;
  const Vector4 da =
      bones_chain_len * clamp(Vector4::zero(),
                              Vector4(_job.soften, 0.0f, 0.0f, 0.0f),
                              _setup.one);
  const FloatInVec ds = bones_chain_len - da.getX();

  // Sotftens target position if it is further than a ratio (_soften) of the
  // whole bone chain length. Needs to check also that ds and
  // start_target_original_ss_len2 are != 0, because they're used as a
  // denominator.
  // x = start_target_original_ss_len > da
  // y = start_target_original_ss_len > 0
  // z = start_target_original_ss_len > bone_len_diff_abs
  // w = ds                           > 0
  Vector4 left(start_target_original_ss_len);
  left.setW(ds);
  Vector4 right(da);
  right.setZ(bone_len_diff_abs);
  const Vector4Int comp = cmpGt(left, right);
  const int comp_mask = MoveMask(comp);

  // xyw all 1, z is untested.
  if ((comp_mask & 0xb) == 0xb) {
    // Finds interpolation ratio (aka alpha).
    const FloatInVec alpha = (start_target_original_ss_len - da.getX()) * rcpEst(ds);
    // Approximate an exponential function with : 1-(3^4)/(alpha+3)^4
    // The derivative must be 1 for x = 0, and y must never exceeds 1.
    // Negative x aren't used.
    const FloatInVec three(3.f);
    Vector4 op(three);
    op.setY(alpha + three);
    const Vector4 op2 = mulPerElem(op, op);
    const Vector4 op4 = mulPerElem(op2, op2);
    const FloatInVec ratio = op4.getX() * rcpEst(op4.getY());

    // Recomputes start_target_ss vector and length.
    const FloatInVec start_target_ss_len = da.getX() + ds - ds * ratio;
    *_start_target_ss_len2 = start_target_ss_len * start_target_ss_len;
    *_start_target_ss =
        start_target_original_ss *
        (start_target_ss_len * rcpEst(start_target_original_ss_len));
  } else {
    *_start_target_ss = start_target_original_ss;
    *_start_target_ss_len2 = start_target_original_ss_len2;
  }

  // The maximum distance we can reach is the soften bone chain length: da
  // (stored in !x). The minimum distance we can reach is the absolute value of
  // the difference of the 2 bone lengths, |d1−d2| (stored in z). x is 0 and z
  // is 1, yw are untested.
  return (comp_mask & 0x5) == 0x4;
}

Quat ComputeMidJoint(const IKTwoBoneJob& _job,
                     const IKConstantSetup& _setup,
                     const FloatInVec _start_target_ss_len2) {
  // Computes expected angle at mid_ss joint, using law of cosine (generalized
  // Pythagorean).
  // c^2 = a^2 + b^2 - 2ab cosC
  // cosC = (a^2 + b^2 - c^2) / 2ab
  // Computes both corrected and initial mid joint angles
  // cosine within a single SimdFloat4 (corrected is x component, initial is y).
  const Vector4 start_mid_end_sum_ss_len2(
      _setup.start_mid_ss_len2 + _setup.mid_end_ss_len2);
  const Vector4 start_mid_end_ss_half_rlen(
      FloatInVec(.5f) *
      rSqrtEstNR(_setup.start_mid_ss_len2 * _setup.mid_end_ss_len2));
  // Cos value needs to be clamped, as it will exit expected range if
  // start_target_ss_len2 is longer than the triangle can be (start_mid_ss +
  // mid_end_ss).
  const Vector4 mid_cos_angles_unclamped =
      mulPerElem( (start_mid_end_sum_ss_len2 -
       Vector4(_start_target_ss_len2).setY(_setup.start_end_ss_len2)),
      start_mid_end_ss_half_rlen);
  const Vector4 mid_cos_angles =
      clamp(_setup.m_one, mid_cos_angles_unclamped, _setup.one);

  // Computes corrected angle
  const Vector4 mid_angles = aCos(mid_cos_angles);
  const FloatInVec mid_corrected_angle = mid_angles.getX();

  // Computes initial angle.
  // The sign of this angle needs to be decided. It's considered negative if
  // mid-to-end joint is bent backward (mid_axis direction dictates valid
  // bent direction).
  const Vector3 bent_side_ref = cross(_setup.start_mid_ms, _job.mid_axis);
  const BoolInVec bent_side_flip =
      dot(bent_side_ref, _setup.mid_end_ms) < FloatInVec(0.0f);
  const FloatInVec mid_initial_angle = xorPerElem(mid_angles.getY(),
                                       (bent_side_flip & _setup.mask_sign));

  // Finally deduces initial to corrected angle difference.
  const FloatInVec mid_angles_diff = mid_corrected_angle - mid_initial_angle;

  // Builds queternion.
  return Quat::rotation(mid_angles_diff, _job.mid_axis);
}

Quat ComputeStartJoint(const IKTwoBoneJob& _job, const IKConstantSetup& _setup,
                       const Quat& _mid_rot_ms, const Vector3 _start_target_ss,
                       const FloatInVec _start_target_ss_len2) {
  // Pole vector in start joint space (_ss)
  const Vector3 pole_ss = (_setup.inv_start_joint * _job.pole_vector).getXYZ();

  // start_mid_ss with quaternion mid_rot_ms applied.
  const Vector3 mid_end_ss_final =
      (_setup.inv_start_joint *
      (*_job.mid_joint * rotate(_mid_rot_ms, _setup.mid_end_ms))).getXYZ();
  const Vector3 start_end_ss_final = _setup.start_mid_ss + mid_end_ss_final;

  // Quaternion for rotating the effector onto the target
  const Quat end_to_target_rot_ss =
      Quat::fromVectors(start_end_ss_final, _start_target_ss);

  // Calculates rotate_plane_ss quaternion which aligns joint chain plane to
  // the reference plane (pole vector). This can only be computed if start
  // target axis is valid (not 0 length)
  // -------------------------------------------------
  Quat start_rot_ss = end_to_target_rot_ss;
  if (_start_target_ss_len2 > FloatInVec(0.0f)) {
    // Computes each plane normal.
    const Vector3 ref_plane_normal_ss =
        cross(_start_target_ss, pole_ss);
    const FloatInVec ref_plane_normal_ss_len2 =
        lengthSqr(ref_plane_normal_ss);
    // Computes joint chain plane normal, which is the same as mid joint axis
    // (same triangle).
    const Vector3 mid_axis_ss =
        (_setup.inv_start_joint *
                        (*_job.mid_joint * _job.mid_axis)).getXYZ();
    const Vector3 joint_plane_normal_ss =
        rotate(end_to_target_rot_ss,mid_axis_ss);
    const FloatInVec joint_plane_normal_ss_len2 =
        lengthSqr(joint_plane_normal_ss);
    // Computes all reciprocal square roots at once.
    const Vector3 rsqrts =
        rSqrtEstNR(Vector3(_start_target_ss_len2, ref_plane_normal_ss_len2,
                        joint_plane_normal_ss_len2));

    // Computes angle cosine between the 2 normalized normals.
    const Vector4 rotate_plane_cos_angle(
        dot(ref_plane_normal_ss * rsqrts.getY(),
                        joint_plane_normal_ss * rsqrts.getZ()));

    // Computes rotation axis, which is either start_target_ss or
    // -start_target_ss depending on rotation direction.
    const Vector3 rotate_plane_axis_ss = _start_target_ss * rsqrts.getX();
    const FloatInVec start_axis_flip =
        andPerElem(dot(joint_plane_normal_ss, pole_ss), _setup.mask_sign);
    const Vector3 rotate_plane_axis_flipped_ss =
        xorPerElem(rotate_plane_axis_ss, start_axis_flip);

    // Builds quaternion along rotation axis.
    const Quat rotate_plane_ss = Quat::fromAxisCosAngle(
        rotate_plane_axis_flipped_ss,
        clamp(_setup.m_one, rotate_plane_cos_angle, _setup.one).getX());

    if (_job.twist_angle != 0.f) {
      // If a twist angle is provided, rotation angle is rotated along
      // rotation plane axis.
      const Quat twist_ss = Quat::rotation(_job.twist_angle, rotate_plane_axis_ss );
      start_rot_ss = twist_ss * rotate_plane_ss * end_to_target_rot_ss;
    } else {
      start_rot_ss = rotate_plane_ss * end_to_target_rot_ss;
    }
  }
  return start_rot_ss;
}

void WeightOutput(const IKTwoBoneJob& _job, const IKConstantSetup& _setup,
                  const Quat& _start_rot,
                  const Quat& _mid_rot) {
  const FloatInVec zero(0.0f);

  // Fix up quaternions so w is always positive, which is required for NLerp
  // (with identity quaternion) to lerp the shortest path.
  const Vector4 start_rot_fu =
      xorPerElem(Vector4(_start_rot),
          And(_setup.mask_sign, _start_rot.getW() < zero));
  const Vector4 mid_rot_fu = xorPerElem(
      Vector4(_mid_rot), And(_setup.mask_sign, _mid_rot.getW() < zero));

  if (_job.weight < 1.f) {
    // NLerp start and mid joint rotations.
    const Vector4 identity = Vector4::wAxis();
    const FloatInVec simd_weight = max(zero, FloatInVec(_job.weight));

    // Lerp
    const Vector4 start_lerp = lerp(simd_weight, identity, start_rot_fu);
    const Vector4 mid_lerp = lerp(simd_weight, identity, mid_rot_fu);


    Vector4 lengths(lengthSqr(start_lerp));
    lengths.setY(lengthSqr(mid_lerp));

    // Normalize
    const Vector4 rsqrts = rSqrtEstNR(lengths);
    *_job.start_joint_correction = Quat(start_lerp * rsqrts.getX());
    *_job.mid_joint_correction = Quat(mid_lerp * rsqrts.getY());
  } else {
    // Quatenions don't need interpolation
    *_job.start_joint_correction = Quat(start_rot_fu);
    *_job.mid_joint_correction = Quat(mid_rot_fu);
  }
}
}  // namespace

bool IKTwoBoneJob::Run() const {
  if (!Validate()) {
    return false;
  }

  // Early out if weight is 0.
  if (weight <= 0.f) {
    // No correction.
    *start_joint_correction = *mid_joint_correction =
        Quat::identity();
    // Target isn't reached.
    if (reached) {
      *reached = false;
    }
    return true;
  }

  // Prepares constant ik data.
  const IKConstantSetup setup(*this);

  // Finds soften target position.
  Vector3 start_target_ss;
  FloatInVec start_target_ss_len2;
  const bool lreached =
      SoftenTarget(*this, setup, &start_target_ss, &start_target_ss_len2);
  if (reached) {
    *reached = lreached && weight >= 1.f;
  }

  // Calculate mid_rot_local quaternion which solves for the mid_ss joint
  // rotation.
  const Quat mid_rot_ms =
      ComputeMidJoint(*this, setup, start_target_ss_len2);

  // Calculates end_to_target_rot_ss quaternion which solves for effector
  // rotating onto the target.
  const Quat start_rot_ss = ComputeStartJoint(
      *this, setup, mid_rot_ms, start_target_ss, start_target_ss_len2);

  // Finally apply weight and output quaternions.
  WeightOutput(*this, setup, start_rot_ss, mid_rot_ms);

  return true;
}
}  // namespace animation
}  // namespace ozz
