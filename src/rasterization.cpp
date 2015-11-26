/**
This file is part of Active Appearance Models (AAM).

Copyright Christoph Heindl 2015
Copyright Sebastian Zambal 2015

AAM is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

AAM is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with AAM.  If not, see <http://www.gnu.org/licenses/>.
*/


#include <aam/rasterization.h>
#include <aam/barycentrics.h>
#include <aam/map.h>
#include <aam/bilinear.h>
#include <opencv2/opencv.hpp>
#include <iostream>

namespace aam {


    MatrixX rasterizeShape(
        Eigen::Ref<const RowVectorX> pointsInterleaved,
        Eigen::Ref<const RowVectorXi> triangleIds,
        MatrixX::Index imageWidth, MatrixX::Index imageHeight, Scalar shapeScale)
    {
        MatrixX points = fromInterleaved<Scalar>(pointsInterleaved) * shapeScale;
        MatrixX::Index nTriangles = triangleIds.size() / 3;

        std::vector<RowVector3> coords;

        for (MatrixX::Index tri = 0; tri < nTriangles; ++tri) {
            auto p0 = points.row(triangleIds(tri * 3 + 0));
            auto p1 = points.row(triangleIds(tri * 3 + 1));
            auto p2 = points.row(triangleIds(tri * 3 + 2));

            ParametrizedTriangle pt(p0, p1, p2);

            for (MatrixX::Index y = 0; y < imageHeight; ++y) {
                for (MatrixX::Index x = 0; x < imageWidth; ++x) {
                    RowVector2 p((x + Scalar(0.5)), (y + Scalar(0.5)));
                    RowVector2 bary = pt.baryAt(p);

                    if (pt.isBaryInside(bary)) {
                        coords.push_back(RowVector3(aam::Scalar(tri), bary(0), bary(1)));
                    }
                }
            }
        }

        MatrixX result(coords.size(), 3);

        for (size_t i = 0; i < coords.size(); ++i) {
            result.row(i) = coords[i];
        }
        
        return result;
        
    }
    
    void writeShapeImage(
        Eigen::Ref<const RowVectorX> normalizedShape,
        Eigen::Ref<const RowVectorXi> triangleIds,
        Eigen::Ref<const MatrixX> barycentricSamplePositions,
        Scalar shapeScale,
        cv::InputArray colorsAtSamplePositions_,
        cv::InputOutputArray dst_)
    {
        cv::Mat colors = colorsAtSamplePositions_.getMat();
        cv::Mat dst = dst_.getMat();
        
        // Loop over sample positions and write colors
        RowVectorX points = normalizedShape * shapeScale;
        
        int triIdLast = -1;
        ParametrizedTriangle pt;
        for (MatrixX::Index i = 0; i < barycentricSamplePositions.rows(); ++i) {
            auto rb = barycentricSamplePositions.row(i);

            int triId = (int)rb(0);
            if (triId != triIdLast) {
                pt.updateVertices(
                    points.segment(2 * triangleIds(triId * 3 + 0), 2),
                    points.segment(2 * triangleIds(triId * 3 + 1), 2),
                    points.segment(2 * triangleIds(triId * 3 + 2), 2));                
                triIdLast = triId;
            }
            
            auto p = pt.pointAt(rb.rightCols(2));
            auto pi = (p - RowVector2::Constant(Scalar(0.5))).cast<MatrixX::Index>();
            
            if ((pi.array() >= 0).all() && pi(0) < dst.cols && pi(1) < dst.rows) {
                cvSet2D(&(IplImage)dst, pi(1), pi(0), cvGet2D(&(IplImage)colors, i, 0));
            }
        }
    }

    void readShapeImage(
        Eigen::Ref<const RowVectorX> normalizedShape,
        Eigen::Ref<const RowVectorXi> triangleIds,
        Eigen::Ref<const MatrixX> barycentricSamplePositions,
        Scalar shapeScale,
        cv::InputArray img_,
        cv::InputOutputArray dst_)
    {
        dst_.create(barycentricSamplePositions.rows(), 1, img_.type());

        cv::Mat dst = dst_.getMat();
        cv::Mat img = img_.getMat();

        RowVectorX points = normalizedShape * shapeScale;

        int triIdLast = -1;
        ParametrizedTriangle pt;
        for (MatrixX::Index i = 0; i < barycentricSamplePositions.rows(); ++i) {
            auto rb = barycentricSamplePositions.row(i);

            int triId = (int)rb(0);
            if (triId != triIdLast) {
                pt.updateVertices(
                    points.segment(2 * triangleIds(triId * 3+0), 2),
                    points.segment(2 * triangleIds(triId * 3+1), 2),
                    points.segment(2 * triangleIds(triId * 3+2), 2));
                triIdLast = triId;
            }

            auto p = pt.pointAt(rb.rightCols(2));

            cvSet2D(&(IplImage)dst, i, 0, bilinear(img, p(1), p(0)));
        }
    }
    
}