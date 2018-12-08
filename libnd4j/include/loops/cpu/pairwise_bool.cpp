/*******************************************************************************
 * Copyright (c) 2015-2018 Skymind, Inc.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Apache License, Version 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0.
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 * License for the specific language governing permissions and limitations
 * under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 ******************************************************************************/

//
// Created by remote on 2018-09-20.
//

#include <loops/pairwise_bool.h>
#include <types/types.h>
#include <OmpLaunchHelper.h>

using namespace simdOps;

namespace functions {
    namespace pairwise_transforms {

        template <typename X, typename Y>
        void PairWiseBoolTransform<X, Y>::exec(
                const int opNum,
                void *x,
                Nd4jLong xEws,
                void *y,
                Nd4jLong yEws,
                void *z,
                Nd4jLong zEws,
                void *extraParams,
                Nd4jLong n) {
            DISPATCH_BY_OPNUM_TT(exec, PARAMS(x,
                                               xEws,
                                               y,
                                               yEws,
                                               z,
                                               zEws,
                                               extraParams,
                                               n), PAIRWISE_BOOL_OPS);
        };



        template <typename X, typename Z>
        template <typename OpType>
        void PairWiseBoolTransform<X, Z>::exec(void *vx,
                                              Nd4jLong xEws,
                                              void *vy,
                                              Nd4jLong yEws,
                                              void *vz,
                                              Nd4jLong zEws,
                                              void *vextraParams,
                                              const Nd4jLong n) {
            
            auto x = reinterpret_cast<X *>(vx);
            auto y = reinterpret_cast<X *>(vy);
            auto z = reinterpret_cast<Z *>(vz);
            auto extraParams = reinterpret_cast<X *>(vextraParams);

            nd4j::OmpLaunchHelper info(n);

            if (xEws == 1 && yEws == 1 && zEws == 1) {            

                #pragma omp parallel num_threads(info._numThreads) if (info._numThreads > 1) default(shared)
                {                
                    auto threadNum = omp_get_thread_num();
                    Nd4jLong threadOffset = info.getThreadOffset(threadNum);        
                    auto xi = x + threadOffset;
                    auto yi = y + threadOffset;
                    auto zi = z + threadOffset;
                    #pragma omp simd
                    for (Nd4jLong i = 0; i < info.getItersPerThread(threadNum); i++) 
                        zi[i] = OpType::op(xi[i], yi[i], extraParams);
                }
            }
            else {

                #pragma omp parallel num_threads(info._numThreads) if (info._numThreads > 1) default(shared)
                {                
                    auto threadNum = omp_get_thread_num();
                    Nd4jLong threadOffset = info.getThreadOffset(threadNum);        
                    auto xi = x + xEws*threadOffset;
                    auto yi = y + yEws*threadOffset;
                    auto zi = z + zEws*threadOffset;
                    #pragma omp simd
                    for (Nd4jLong i = 0; i < info.getItersPerThread(threadNum); i++) 
                        zi[i*zEws] = OpType::op(xi[i*xEws], yi[i*yEws], extraParams);
                }
            }
        }

        template <typename X, typename Y>
        void PairWiseBoolTransform<X, Y>::exec(
                const int opNum,
                void *x,
                Nd4jLong *xShapeInfo,
                void *y,
                Nd4jLong *yShapeInfo,
                void *z,
                Nd4jLong *zShapeInfo,
                void *extraParams) {
            DISPATCH_BY_OPNUM_TT(exec, PARAMS(x,
                                              xShapeInfo,
                                              y,
                                              yShapeInfo,
                                              z,
                                              zShapeInfo,
                                              extraParams),
                                 PAIRWISE_BOOL_OPS);
        };


        template <typename X, typename Z>
        template <typename OpType>
        void PairWiseBoolTransform<X, Z>::exec(void *vx, Nd4jLong* xShapeInfo,
                                            void *vy, Nd4jLong* yShapeInfo,
                                            void *vz, Nd4jLong* zShapeInfo,
                                            void *vextraParams) {

            auto x = reinterpret_cast<X *>(vx);
            auto y = reinterpret_cast<X *>(vy);
            auto z = reinterpret_cast<Z *>(vz);
            auto extraParams = reinterpret_cast<X *>(vextraParams);

            auto n = shape::length(xShapeInfo);
            auto xEws = shape::elementWiseStride(xShapeInfo);
            auto yEws = shape::elementWiseStride(yShapeInfo);
            auto zEws = shape::elementWiseStride(zShapeInfo);

            nd4j::OmpLaunchHelper info(n);

            if (shape::isScalar(yShapeInfo)) {

                if (xEws == 1 && zEws == 1) {

                    #pragma omp parallel num_threads(info._numThreads) if (info._numThreads > 1) default(shared)
                    {                
                        auto threadNum = omp_get_thread_num();
                        Nd4jLong threadOffset = info.getThreadOffset(threadNum);        
                        auto xi = x + threadOffset;                        
                        auto zi = z + threadOffset;
                        #pragma omp simd
                        for (Nd4jLong i = 0; i < info.getItersPerThread(threadNum); i++)                             
                            zi[i] = OpType::op(xi[i], y[0], extraParams);
                    }
                } 
                else {
                    
                    #pragma omp parallel num_threads(info._numThreads) if (info._numThreads > 1) default(shared)
                    {                
                        auto threadNum = omp_get_thread_num();
                        Nd4jLong threadOffset = info.getThreadOffset(threadNum);        
                     
                        #pragma omp simd
                        for(Nd4jLong i = 0; i < info.getItersPerThread(threadNum); i++)  {
                            auto xOffset = shape::getIndexOffset(i+threadOffset, xShapeInfo, n);
                            auto zOffset = shape::getIndexOffset(i+threadOffset, zShapeInfo, n);
                            z[zOffset] = OpType::op(x[xOffset], y[0], extraParams);
                        }
                    }
                }
                return;
            }

            bool sameShape = shape::shapeEquals(shape::rank(xShapeInfo), shape::shapeOf(xShapeInfo), shape::rank(yShapeInfo), shape::shapeOf(yShapeInfo));

            if (xEws >= 1 && yEws >= 1 && zEws >= 1 &&
                shape::order(xShapeInfo) == shape::order(yShapeInfo) &&
                shape::order(zShapeInfo) == shape::order(xShapeInfo) &&
                sameShape &&  xEws == yEws) {

                exec<OpType>(x, xEws, y, yEws, z, zEws, extraParams, n);
            }
                
            else if (!sameShape && shape::order(xShapeInfo) == shape::order(yShapeInfo) &&
                     shape::order(zShapeInfo) == shape::order(xShapeInfo) && xEws >= 1 &&
                     yEws >= 1 && zEws >= 1 && xEws == yEws) { //not same shape

                exec<OpType>(x, xEws, y, yEws, z, zEws, extraParams, shape::length(yShapeInfo));
            }

            else {
                
                if(vx == vz) {

                    #pragma omp parallel num_threads(info._numThreads) if (info._numThreads > 1) default(shared)
                    {                
                        auto threadNum = omp_get_thread_num();
                        Nd4jLong threadOffset = info.getThreadOffset(threadNum);        
                     
                        #pragma omp simd
                        for (Nd4jLong i = 0; i < info.getItersPerThread(threadNum); i++)  {
                            auto xOffset = shape::getIndexOffset(i+threadOffset, xShapeInfo, n);
                            auto yOffset = shape::getIndexOffset(i+threadOffset, yShapeInfo, n);
                            z[xOffset] = OpType::op(x[xOffset], y[yOffset], extraParams);
                        }
                    }
                }
                else {
                    
                    #pragma omp parallel num_threads(info._numThreads) if (info._numThreads > 1) default(shared)
                    {                
                        auto threadNum = omp_get_thread_num();
                        Nd4jLong threadOffset = info.getThreadOffset(threadNum);        
                     
                        #pragma omp simd
                        for (Nd4jLong i = 0; i < info.getItersPerThread(threadNum); i++)  {
                            auto xOffset = shape::getIndexOffset(i+threadOffset, xShapeInfo, n);
                            auto yOffset = shape::getIndexOffset(i+threadOffset, yShapeInfo, n);
                            auto zOffset = shape::getIndexOffset(i+threadOffset, zShapeInfo, n);
                            z[zOffset] = OpType::op(x[xOffset], y[yOffset], extraParams);
                        }
                    }
                }
            }
        }

        BUILD_DOUBLE_TEMPLATE(template class ND4J_EXPORT PairWiseBoolTransform, , LIBND4J_TYPES, BOOL_TYPES);
    }
}
