#pragma once
#include "graph/IDirectedGraph.h"
#include <map>
#include <memory>
#include <optional>
#include <string>

namespace gladius::nodes
{
    class NodeBase;
    class Begin;
    class End;

    // 3mf
    class ConstantScalar;
    class ConstantVector;
    class ConstantMatrix;

    class ComposeVector;
    class ComposeMatrix;
    class ComposeMatrixFromColumns;
    class ComposeMatrixFromRows;
    class DecomposeMatrix;

    class Addition;
    class Multiplication;
    class Subtraction;
    class Division;

    class DotProduct;
    class CrossProduct;
    class MatrixVectorMultiplication;
    class Transpose;
    class Inverse;
    class Sine;
    class Cosine;
    class SinH;
    class CosH;
    class TanH;
    class Tangent;
    class ArcSin;
    class ArcCos;
    class ArcTan;
    class ArcTan2;
    class Min;
    class Max;
    class Abs;
    class Sqrt;
    class Fmod;
    class Pow;
    class Exp;
    class Log;
    class Log2;
    class Log10;
    class Select;
    class Clamp;
    class Round;
    class Ceil;
    class Floor;
    class Sign;
    class Fract;
    class SignedDistanceToMesh;
    class SignedDistanceToBeamLattice;
    class FunctionCall;
    class FunctionGradient;
    class NormalizeDistanceField;
    class Length;
    class DecomposeVector;
    class Resource;
    class VectorFromScalar;
    class UnsignedDistanceToMesh;
    class Mod;

    // gladius extensions
    class Mix;
    class Transformation;
    class ImageSampler;
    class BoxMinMax;

    class Port;
    class Visitor;
    class NodeBase;
    class Model;
    class Assembly;
    using UniqueAssembly = std::unique_ptr<Assembly>;

    using ParameterName = std::string;
    using NodeName = std::string;
    using PortName = std::string;

    using NodeId = graph::Identifier;
    using PortId = graph::Identifier;
    using ParameterId = graph::Identifier;

    using OptionalPortId = std::optional<PortId>;
    using OptionalPortName = std::optional<PortName>;
    using ModelName = std::string;

    using NodeTypes = std::tuple<Begin,
                                 End,
                                 ConstantScalar,
                                 ConstantVector,
                                 ConstantMatrix,
                                 ComposeVector,
                                 ComposeMatrix,
                                 ComposeMatrixFromColumns,
                                 ComposeMatrixFromRows,
                                 Addition,
                                 Multiplication,
                                 Subtraction,
                                 Division,
                                 DotProduct,
                                 CrossProduct,
                                 MatrixVectorMultiplication,
                                 Transpose,
                                 Sine,
                                 Cosine,
                                 Tangent,
                                 ArcSin,
                                 ArcCos,
                                 ArcTan,
                                 Min,
                                 Max,
                                 Abs,
                                 Sqrt,
                                 Fmod,
                                 Pow,
                                 SignedDistanceToMesh,
                                 SignedDistanceToBeamLattice,
                                 FunctionCall,
                                 FunctionGradient,
                                 NormalizeDistanceField,
                                 Transformation,
                                 Length,
                                 DecomposeVector,
                                 Resource,
                                 ImageSampler,
                                 DecomposeMatrix,
                                 Inverse,
                                 ArcTan2,
                                 Exp,
                                 Log,
                                 Log2,
                                 Log10,
                                 Select,
                                 Clamp,
                                 SinH,
                                 CosH,
                                 TanH,
                                 Round,
                                 Ceil,
                                 Floor,
                                 Sign,
                                 Fract,
                                 VectorFromScalar,
                                 UnsignedDistanceToMesh,
                                 Mod,
                                 BoxMinMax>;

    enum class Category
    {
        Internal,
        Primitive,
        Transformation,
        Alteration,
        BoolOperation,
        Lattice,
        Misc,
        Math,
        Export,
        _3mf
    };

    using CategoryNamesT = std::map<Category, std::string>;
    static const CategoryNamesT CategoryNames{{Category::Internal, "Internal"},
                                              {Category::Primitive, "Primitive"},
                                              {Category::Transformation, "Transformation"},
                                              {Category::Alteration, "Alteration"},
                                              {Category::BoolOperation, "Bool Operations"},
                                              {Category::Lattice, "Lattice Structures"},
                                              {Category::Misc, "Misc"},
                                              {Category::Math, "Math"},
                                              {Category::Export, "Export"},
                                              {Category::_3mf, "3mf"}};

    struct FieldNames
    {
        static auto constexpr Pos = "pos";
        static auto constexpr Shape = "shape";

        static auto constexpr Point = "point";
        static auto constexpr Distance = "distance";

        static auto constexpr Color = "color";
        static auto constexpr ColorA = "ColorA";
        static auto constexpr ColorB = "ColorB";
        static auto constexpr A = "A";
        static auto constexpr B = "B";
        static auto constexpr C = "C";
        static auto constexpr D = "D";
        static auto constexpr Radius = "radius";
        static auto constexpr Offset = "offset";
        static auto constexpr Transformation = "Transformation";
        static auto constexpr Angle = "angle";
        static auto constexpr Thickness = "thickness";
        static auto constexpr Dimensions = "dimensions";
        static auto constexpr Sum = "sum";
        static auto constexpr Value = "value";
        static auto constexpr Result = "result";
        static auto constexpr Scaling = "scaling";
        static auto constexpr Vector = "vector";
        static auto constexpr Gradient = "gradient";
        static auto constexpr Magnitude = "magnitude";
        static auto constexpr Matrix = "matrix";
        static auto constexpr X = "x";
        static auto constexpr Y = "y";
        static auto constexpr Z = "z";
        static auto constexpr Domain = "domain";
        static auto constexpr Min = "min";
        static auto constexpr Max = "max";

        static auto constexpr Base = "base";
        static auto constexpr Exponent = "exponent";

        static auto constexpr Radius1 = "radius_1";
        static auto constexpr Radius2 = "radius_2";
        static auto constexpr Radius3 = "radius_3";
        static auto constexpr Radius4 = "radius_4";
        static auto constexpr Number = "number";

        static auto constexpr Length = "length";
        static auto constexpr Width = "width";
        static auto constexpr Height = "height";
        static auto constexpr Depth = "depth";
        static auto constexpr NumSheets = "numsheets";
        static auto constexpr Size = "size";
        static auto constexpr RadPerMM = "radPerMM";
        static auto constexpr pitch = "pitch";
        static auto constexpr Ratio = "ratio";

        static auto constexpr Start = "start";
        static auto constexpr End = "end";
        // static auto constexpr Part = "part";
        static auto constexpr FunctionId = "functionID";
        static auto constexpr StepSize = "step";
        static auto constexpr NormalizedGradient = "normalizedgradient";
        static auto constexpr Epsilon = "epsilon";
        static auto constexpr MaxDistance = "maxdistance";
        static auto constexpr Source = "source";
        static auto constexpr sharpness = "sharpness";

        static auto constexpr Filename = "filename";
        static auto constexpr ResourceId = "resourceid";
        static auto constexpr Mesh = "mesh";
        static auto constexpr BeamLattice = "beamlatticeid";

        static auto constexpr Image = "image";
        static auto constexpr Red = "red";
        static auto constexpr Green = "green";
        static auto constexpr Blue = "blue";
        static auto constexpr Alpha = "alpha";
        static auto constexpr UVW = "uvw";
        static auto constexpr TileStyleU = "tilestyle_u";
        static auto constexpr TileStyleV = "tilestyle_v";
        static auto constexpr TileStyleW = "tilestyle_w";
        static auto constexpr Filter = "filter";

        // gear
        static auto constexpr PressureAngle = "pressureAngle";
        static auto constexpr NumberOfTeeth = "numberOfTeeth";
        static auto constexpr Modulus = "modulus";
        static auto constexpr ClearanceFactor = "clearanceFactor";

        static auto constexpr OriginX = "origin_x";
        static auto constexpr OriginY = "origin_y";
        // Layer
        static auto constexpr Layerthickness = "layerthickness";
        static auto constexpr FirstLayerThickness = "firstlayerthickness";
        static auto constexpr StartHeight = "startheight";
        static auto constexpr EndHeight = "endheight";

        // Slice parameter
        static auto constexpr LineWidth = "linewidth";
        static auto constexpr ContourOffset = "offset";

        // Build parameter
        static auto constexpr Speed = "speed";
        static auto constexpr JumpSpeed = "jumpspeed";
        static auto constexpr RetractionLength = "retractionlength";
        static auto constexpr RetractionMoveBack = "retractionmoveback";
        static auto constexpr RetractionMinJumpLength = "retractionminjumplength";
        static auto constexpr RetractionSpeed = "retractionspeed";
        static auto constexpr ExtrusionMultiplicator = "extrusionmultiplicator";

        // Build Parameter sets
        static auto constexpr BuildParameter = "buildparameter";
        static auto constexpr FirstLayer = "firstlayer";

        // CodeReps
        static auto constexpr Code = "code";

        // Matrix
        static auto constexpr M00 = "m00";
        static auto constexpr M01 = "m01";
        static auto constexpr M02 = "m02";
        static auto constexpr M03 = "m03";
        static auto constexpr M10 = "m10";
        static auto constexpr M11 = "m11";
        static auto constexpr M12 = "m12";
        static auto constexpr M13 = "m13";
        static auto constexpr M20 = "m20";
        static auto constexpr M21 = "m21";
        static auto constexpr M22 = "m22";
        static auto constexpr M23 = "m23";
        static auto constexpr M30 = "m30";
        static auto constexpr M31 = "m31";
        static auto constexpr M32 = "m32";
        static auto constexpr M33 = "m33";

        // columns
        static auto constexpr Col0 = "col0";
        static auto constexpr Col1 = "col1";
        static auto constexpr Col2 = "col2";
        static auto constexpr Col3 = "col3";

        // rows
        static auto constexpr Row0 = "row0";
        static auto constexpr Row1 = "row1";
        static auto constexpr Row2 = "row2";
        static auto constexpr Row3 = "row3";
    };
} // namespace gladius::nodes // namespace gladius::nodes
