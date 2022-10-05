//----------------------------------------------------------------------------//
//                                                                            //
// ozz-animation is hosted at http://github.com/guillaumeblanc/ozz-animation  //
// and distributed under the MIT License (MIT).                               //
//                                                                            //
// Copyright (c) Guillaume Blanc                                              //
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

#include "../../../include/ozz/animation/runtime/ik_aim_job.h"

namespace ozz {
    namespace animation {
        IKAimJob::IKAimJob()
            : target(0.0f),
            forward(Vector3::xAxis()),
            offset(0.0f),
            up(Vector3::yAxis()),
            pole_vector(Vector3::yAxis()),
            twist_angle(0.f),
            weight(1.f),
            joint(NULL),
            joint_correction(NULL),
            reached(NULL) {}

        bool IKAimJob::Validate() const {
            bool valid = true;
            valid &= joint != NULL;
            valid &= joint_correction != NULL;
            valid &= isNormalizedEst(forward);
            return valid;
        }

        namespace {

            // When there's an offset, the forward vector needs to be recomputed.
            // The idea is to find the vector that will allow the point at offset position
            // to aim at target position. This vector starts at joint position. It ends on a
            // line perpendicular to pivot-offset line, at the intersection with the sphere
            // defined by target position (centered on joint position). See geogebra
            // diagram: media/doc/src/ik_aim_offset.ggb
            bool ComputeOffsettedForward(Vector3 _forward, Vector3 _offset, Vector3 _target,
                Vector3* _offsetted_forward) {
                // assert(ozz::math::AreAllTrue1(ozz::math::IsNormalizedEst3(_forward)));
                // AO -- projected offset vector onto the normalized forward vector.
                // ACl2 -- Compute square length of ac using Pythagorean theorem.
                // r2 -- Square length of target vector, aka circle radius.
#if !VECTORMATH_MODE_SCALAR
                const FloatInVec AOl = dot(_forward, _offset);
                const FloatInVec ACl2 = lengthSqr(_offset) - AOl * AOl;
                const FloatInVec r2 = lengthSqr(_target);
#else
                const float AOl = dot(_forward, _offset);
                const float ACl2 = lengthSqr(_offset) - AOl * AOl;
                const float r2 = lengthSqr(_target);
#endif

                // If offset is outside of the sphere defined by target length, the target
                // isn't reachable.
                if (ACl2 > r2) {
                    return false;
                }

                // AIl -- the length of the vector from offset to sphere intersection.
#if !VECTORMATH_MODE_SCALAR
                const FloatInVec AIl = sqrt(r2 - ACl2);
#else
                const float AIl = sqrt(r2 - ACl2);
#endif

                // The distance from offset position to the intersection with the sphere is
                // (AIl - AOl) Intersection point on the sphere can thus be computed.
                * _offsetted_forward = _offset + _forward * (AIl - AOl);

                return true;
            }
        }  // namespace

        bool IKAimJob::Run() const {
            if (!Validate()) {
                return false;
            }

#if !VECTORMATH_MODE_SCALAR
            const FloatInVec zero(0.0f);
#else
            const float zero(0.0f);
#endif

            // If matrices aren't invertible, they'll be all 0 (ozz::math
            // implementation), which will result in identity correction quaternions.
            const Matrix4 inv_joint = inverse(*joint);

            // Computes joint to target vector, in joint local-space (_js).
            const Vector3 joint_to_target_js = (inv_joint * target).getXYZ();
#if !VECTORMATH_MODE_SCALAR
            const FloatInVec joint_to_target_js_len2 = lengthSqr(joint_to_target_js);
#else
            const float joint_to_target_js_len2 = lengthSqr(joint_to_target_js);
#endif

            // Recomputes forward vector to account for offset.
            // If the offset is further than target, it won't be reachable.
            Vector3 offsetted_forward;
            bool lreached = ComputeOffsettedForward(forward, offset, joint_to_target_js,
                &offsetted_forward);
            // Copies reachability result.
            // If offsetted forward vector doesn't exists, target position cannot be
            // aimed.
            if (reached != NULL) {
                *reached = lreached;
            }

            if (!lreached || (joint_to_target_js_len2 == zero)) {
                // Target can't be reached or is too close to joint position to find a
                // direction.
                *joint_correction = Quat::identity();
                return true;
            }

            // Calculates joint_to_target_rot_ss quaternion which solves for
            // offsetted_forward vector rotating onto the target.
            // TODO -- add fromVectors either here or to scalar math
            const Quat joint_to_target_rot_js =
                Quat::fromVectors(offsetted_forward, joint_to_target_js);

            // Calculates rotate_plane_js quaternion which aligns joint up to the pole
            // vector.
            const Vector3 corrected_up_js = rotate(joint_to_target_rot_js, up);

            // Compute (and normalize) reference and pole planes normals.
            const Vector3 pole_vector_js = (inv_joint * pole_vector).getXYZ();
            const Vector3 ref_joint_normal_js = cross(pole_vector_js, joint_to_target_js);
            const Vector3 joint_normal_js = cross(corrected_up_js, joint_to_target_js);

#if !VECTORMATH_MODE_SCALAR
            const FloatInVec ref_joint_normal_js_len2 = lengthSqr(ref_joint_normal_js);
            const FloatInVec joint_normal_js_len2 = lengthSqr(joint_normal_js);
#else
            const float ref_joint_normal_js_len2 = lengthSqr(ref_joint_normal_js);
            const float joint_normal_js_len2 = lengthSqr(joint_normal_js);
#endif

            const Vector4 denoms(joint_to_target_js_len2, joint_normal_js_len2,
                ref_joint_normal_js_len2, zero);

            Vector3 rotate_plane_axis_js;
            Quat rotate_plane_js;
            // Computing rotation axis and plane requires valid normals.
            if (AreAllTrue3(cmpNotEq(denoms, Vector4(zero)))) {
                const Vector4 rsqrts = rSqrtEstNR(denoms);

                // Computes rotation axis, which is either joint_to_target_js or
                // -joint_to_target_js depending on rotation direction.
                rotate_plane_axis_js = joint_to_target_js * rsqrts.getX();

                // Computes angle cosine between the 2 normalized plane normals.
                // TODO -- add scalar bitwise for floats either here or to scalar math
#if !VECTORMATH_MODE_SCALAR
                const FloatInVec rotate_plane_cos_angle = dot(
                    joint_normal_js * rsqrts.getY(), ref_joint_normal_js * rsqrts.getZ());
                const FloatInVec axis_flip = andPerElem(
                    dot(ref_joint_normal_js, corrected_up_js), vector4int::mask_sign());
#else
                const float rotate_plane_cos_angle = dot(
                    joint_normal_js * rsqrts.getY(), ref_joint_normal_js * rsqrts.getZ());
                ScalarFI temp;
                temp.f = dot(ref_joint_normal_js, corrected_up_js);
                temp.i = temp.i & (int32_t)0x80000000;
                const float axis_flip = temp.f;
#endif

                const Vector3 rotate_plane_axis_flipped_js =
                    xorPerElem(rotate_plane_axis_js, axis_flip);

                // Builds quaternion along rotation axis.
#if !VECTORMATH_MODE_SCALAR
                const FloatInVec one(1.0f);
#else
                const float one = 1.0f;
#endif
                rotate_plane_js = Quat::fromAxisCosAngle(
                    rotate_plane_axis_flipped_js, clamp(-one, rotate_plane_cos_angle, one));
            }
            else {
#if !VECTORMATH_MODE_SCALAR
                rotate_plane_axis_js = joint_to_target_js * rSqrtEstNR(denoms.getX());
#else
                rotate_plane_axis_js = joint_to_target_js * rsqrtf(denoms.getX());
#endif
                rotate_plane_js = Quat::identity();
            }

            // Twists rotation plane.
            Quat twisted;
            if (twist_angle != 0.f) {
                // If a twist angle is provided, rotation angle is rotated around joint to
                // target vector.
                const Quat twist_ss = Quat::rotation(twist_angle, rotate_plane_axis_js);
                twisted = twist_ss * rotate_plane_js * joint_to_target_rot_js;
            }
            else {
                twisted = rotate_plane_js * joint_to_target_rot_js;
            }

            // Weights output quaternion.

            // Fix up quaternions so w is always positive, which is required for NLerp
            // (with identity quaternion) to lerp the shortest path.
#if !VECTORMATH_MODE_SCALAR
            const Vector4 twisted_fu = xorPerElem(
                Vector4(twisted), And(vector4int::mask_sign(), twisted.getW() < zero));
#else
            const Vector4 twisted_fu = xorPerElem(
                Vector4(twisted), And(vector4int::mask_sign(),
                    twisted.getW() < zero
                    ? vector4int::all_true()
                    : vector4int::all_false()));
#endif

            if (weight < 1.f) {
                // NLerp start and mid joint rotations.
                const Vector4 identity = Vector4::wAxis();
#if !VECTORMATH_MODE_SCALAR
                const FloatInVec simd_weight = max(zero, FloatInVec(weight));
#else
                const float simd_weight = max(zero, weight);
#endif
                * joint_correction =
                    Quat(normalize(lerp(simd_weight, identity, twisted_fu)));
            }
            else {
                // Quaternion doesn't need interpolation
                *joint_correction = Quat(twisted_fu);
            }
            return true;
        }
    }  // namespace animation
}  // namespace ozz
