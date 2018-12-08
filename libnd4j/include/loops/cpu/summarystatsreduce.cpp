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
// Created by raver119 on 18.12.17.
//

#include <types/types.h>
#include <op_boilerplate.h>
#include <loops/summarystatsreduce.h>
#include <helpers/shape.h>
#include <helpers/TAD.h>

using namespace simdOps;

namespace functions {
    namespace summarystats {


        template <typename X, typename Y>
        Y SummaryStatsReduce<X,Y>::execScalar(const int opNum,
                const bool biasCorrected,
                void *x,
                Nd4jLong *xShapeInfo,
                void *extraParams) {
            RETURNING_DISPATCH_BY_OPNUM_TT(execScalar, PARAMS(biasCorrected, x, xShapeInfo, extraParams), SUMMARY_STATS_OPS);
        }

        template <typename X, typename Y>
        void SummaryStatsReduce<X,Y>::execScalar(const int opNum,
                                              const bool biasCorrected,
                                              void *x,
                                              Nd4jLong *xShapeInfo,
                                              void *extraParams,
                                              void *z,
                                              Nd4jLong *resultShapeInfoBuffer) {
            DISPATCH_BY_OPNUM_TT(execScalar, PARAMS(biasCorrected, x, xShapeInfo, extraParams, z, resultShapeInfoBuffer), SUMMARY_STATS_OPS);
        }

        template <typename X, typename Y>
        void SummaryStatsReduce<X,Y>::exec(const int opNum,
                const bool biasCorrected,
                void *x,
                Nd4jLong *xShapeInfo,
                void *extraParams,
                void *z,
                Nd4jLong *resultShapeInfoBuffer,
                int *dimension,
                int dimensionLength) {
            DISPATCH_BY_OPNUM_TT(exec, PARAMS(biasCorrected, x, xShapeInfo, extraParams, z, resultShapeInfoBuffer, dimension, dimensionLength), SUMMARY_STATS_OPS);
        }

        template <typename X, typename Z>
        template <typename OpType >
        void SummaryStatsReduce<X,Z>::execScalar(const bool biasCorrected,
                                              void *vx,
                                              Nd4jLong *xShapeInfo,
                                              void *vextraParams,
                                              void *vz,
                                              Nd4jLong *resultShapeInfoBuffer) {
            auto z = reinterpret_cast<Z*>(vz);
            z[0] = execScalar<OpType>(biasCorrected, vx, xShapeInfo, vextraParams);
        }

        template <typename X, typename Z>
        template <typename OpType >
        Z SummaryStatsReduce<X,Z>::execScalar(const bool biasCorrected, void *vx, Nd4jLong *xShapeInfo, void *vextraParams) {

            auto x = reinterpret_cast<X *>(vx);
            auto extraParams = reinterpret_cast<Z *>(vextraParams);

            SummaryStatsData<X> startingIndex;
            startingIndex.initialize();
            auto length = shape::length(xShapeInfo);
            auto xEws = shape::elementWiseStride(xShapeInfo);
            if (xEws == 1) {
                for (Nd4jLong i = 0; i < length; i++) {
                    SummaryStatsData<X> curr;
                    curr.initWithValue(x[i]);
                    startingIndex = update(startingIndex, curr, extraParams);
                }

                return OpType::getValue(biasCorrected, startingIndex);
            }
            else {

                for (Nd4jLong i = 0; i < length; i++) {
                                        
                    auto xOffset = shape::getIndexOffset(i, xShapeInfo, length);

                    SummaryStatsData<X> curr;
                    curr.initWithValue(x[xOffset]);
                    startingIndex = update(startingIndex, curr, extraParams);
                }

                return OpType::getValue(biasCorrected, startingIndex);
            }
        }

        template <typename X, typename Z>
        template <typename OpType >
        void SummaryStatsReduce<X,Z>::exec(const bool biasCorrected,
                void *vx,
                Nd4jLong *xShapeInfo,
                void *vextraParams,
                void *vresult,
                Nd4jLong *resultShapeInfoBuffer,
                int *dimension,
                int dimensionLength) {
            auto x = reinterpret_cast<X *>(vx);
            auto z = reinterpret_cast<Z *>(vresult);
            auto extraParams = reinterpret_cast<Z *>(vextraParams);

            if (shape::isScalar(resultShapeInfoBuffer)) {
                z[0] = execScalar<OpType>(biasCorrected, x, xShapeInfo, extraParams);
                return;
            }


            shape::TAD tad(xShapeInfo, dimension, dimensionLength);
            tad.createTadOnlyShapeInfo();
            tad.createOffsets();

            //no-op
            if (tad.dimensionLength < 1)
                return;

            int resultLength = shape::length(resultShapeInfoBuffer);
            //pre squeezed: this is for keeping the pointer to the original
            //shape information for tad offset
            //the squeezed information doesn't render the right strides for
            //tad offset
            if (resultLength == 1 || dimensionLength == shape::rank(xShapeInfo) || tad.wholeThing) {
                z[0] = execScalar<OpType>(biasCorrected, x, xShapeInfo, extraParams);
                return;
            }

            if (!(shape::elementWiseStride(tad.tadOnlyShapeInfo) > 0 && (tad.numTads == 1 || shape::isVector(tad.tadOnlyShapeInfo) ||
                                                                         shape::isScalar(tad.tadOnlyShapeInfo) || tad.wholeThing)) && !(dimensionLength > 1)) {

                /**
                 * The element wise stride belong longs to a reduction index.
                 * When used out of order, we can get rid of the data
                 * dependencies and rely on using the max dimension
                 * specified for stride instead.
                 * Say we take the sum(0,1) along long arr
                 * we can use arr.stride(1) as a representation
                 * along long which to iterate.
                 */

                auto tadShapeShapeInfo = tad.tadOnlyShapeInfo;

                auto xShape = shape::shapeOf(tadShapeShapeInfo);
                auto xStride = shape::stride(tadShapeShapeInfo);
                int rank = shape::rank(tadShapeShapeInfo);
#pragma omp parallel for schedule(guided) default(shared)
                for (int i = 0; i < resultLength; i++) {
                    auto offset = tad.tadOffsets[i];
                    Nd4jLong shapeIter[MAX_RANK];
                    Nd4jLong coord[MAX_RANK];
                    int dim;
                    int rankIter = rank;
                    Nd4jLong xStridesIter[MAX_RANK];
                    auto xPointer = x + offset;
                    SummaryStatsData<X> comp;
                    comp.initWithValue(0.0);
                    if (PrepareOneRawArrayIter<X>(rankIter,
                                                  xShape,
                                                  xPointer,
                                                  xStride,
                                                  &rankIter,
                                                  shapeIter,
                                                  &xPointer,
                                                  xStridesIter) >= 0) {
                        ND4J_RAW_ITER_START(dim, rank, coord, shapeIter); {
                                /* Process the innermost dimension */
                                SummaryStatsData<X> comp2;
                                comp2.initWithValue(xPointer[0]);
                                comp = update(comp, comp2, extraParams);
                            } ND4J_RAW_ITER_ONE_NEXT(dim,
                                                     rank,
                                                     coord,
                                                     shapeIter,
                                                     xPointer,
                                                     xStridesIter);
                    }
                    else {
                        printf("Unable to prepare array\n");
                    }

                    z[i] = OpType::getValue(biasCorrected, comp);
                }
            }
            else {
                if (dimensionLength == 1) {
                    auto tadElementWiseStride = shape::elementWiseStride(tad.tadOnlyShapeInfo);
                    auto tadLength = shape::length(tad.tadOnlyShapeInfo);

#pragma omp parallel for schedule(guided) default(shared)
                    for (int i = 0; i < resultLength; i++) {
                        Nd4jLong baseOffset = tad.tadOffsets[i];
                        SummaryStatsData<X> comp;
                        comp.initWithValue(x[baseOffset]);
// FIXME: reduction to be used here
                        for (int j = 1; j < tadLength; j++) {
                            SummaryStatsData<X> comp2;
                            comp2.initWithValue(x[baseOffset + (tadElementWiseStride * j)]);
                            comp = update(comp, comp2, extraParams);
                        }

                        z[i] = OpType::getValue(biasCorrected, comp);
                    }
                } else {
                    auto tadShapeShapeInfo = tad.tadOnlyShapeInfo;
                    auto tadLength = shape::length(tad.tadOnlyShapeInfo);

#pragma omp parallel for schedule(guided) default(shared)
                    for (int r = 0; r < resultLength; r++) {
                        
                        auto tadOffsetForBlock = tad.tadOffsets[r];
                        SummaryStatsData<X> comp;
                        comp.initWithValue(x[tadOffsetForBlock]);

// FIXME: reduction should be fixed
                        for (int i = 1; i < tadLength; i ++) {                            
                            
                            auto xOffset = tadOffsetForBlock + shape::getIndexOffset(i, tadShapeShapeInfo, tadLength);

                            SummaryStatsData <X> indexVal2;
                            indexVal2.initWithValue(x[xOffset]);

                            comp = update(comp, OpType::op(indexVal2, extraParams), extraParams);
                        }
                        z[r] = OpType::getValue(biasCorrected, comp);
                    }
                }
            }
        }


        BUILD_DOUBLE_TEMPLATE(template class ND4J_EXPORT SummaryStatsReduce, , LIBND4J_TYPES, FLOAT_TYPES);
    }
}