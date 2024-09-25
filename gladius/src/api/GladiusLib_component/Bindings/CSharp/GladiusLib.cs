using System;
using System.Text;
using System.Runtime.InteropServices;

namespace GladiusLib {

    public struct sVector2f
    {
        public Single[] Coordinates;
    }

    public struct sVector3f
    {
        public Single[] Coordinates;
    }

    public struct sChannelMetaInfo
    {
        public Single[] MinPosition;
        public Single[] MaxPosition;
        public Int32[] Size;
        public Int32 RequiredMemory;
    }


    namespace Internal {

        [StructLayout(LayoutKind.Explicit, Size=8)]
        public unsafe struct InternalVector2f
        {
            [FieldOffset(0)] public fixed Single Coordinates[2];
        }

        [StructLayout(LayoutKind.Explicit, Size=12)]
        public unsafe struct InternalVector3f
        {
            [FieldOffset(0)] public fixed Single Coordinates[3];
        }

        [StructLayout(LayoutKind.Explicit, Size=28)]
        public unsafe struct InternalChannelMetaInfo
        {
            [FieldOffset(0)] public fixed Single MinPosition[2];
            [FieldOffset(8)] public fixed Single MaxPosition[2];
            [FieldOffset(16)] public fixed Int32 Size[2];
            [FieldOffset(24)] public Int32 RequiredMemory;
        }


        public class GladiusLibWrapper
        {
            [DllImport("gladius.dll", EntryPoint = "gladiuslib_boundingbox_getmin", CallingConvention=CallingConvention.Cdecl)]
            public unsafe extern static Int32 BoundingBox_GetMin (IntPtr Handle, out InternalVector3f AMin);

            [DllImport("gladius.dll", EntryPoint = "gladiuslib_boundingbox_getmax", CallingConvention=CallingConvention.Cdecl)]
            public unsafe extern static Int32 BoundingBox_GetMax (IntPtr Handle, out InternalVector3f AMax);

            [DllImport("gladius.dll", EntryPoint = "gladiuslib_face_getvertexa", CallingConvention=CallingConvention.Cdecl)]
            public unsafe extern static Int32 Face_GetVertexA (IntPtr Handle, out InternalVector3f AVertexA);

            [DllImport("gladius.dll", EntryPoint = "gladiuslib_face_getvertexb", CallingConvention=CallingConvention.Cdecl)]
            public unsafe extern static Int32 Face_GetVertexB (IntPtr Handle, out InternalVector3f AVertexB);

            [DllImport("gladius.dll", EntryPoint = "gladiuslib_face_getvertexc", CallingConvention=CallingConvention.Cdecl)]
            public unsafe extern static Int32 Face_GetVertexC (IntPtr Handle, out InternalVector3f AVertexC);

            [DllImport("gladius.dll", EntryPoint = "gladiuslib_face_getnormal", CallingConvention=CallingConvention.Cdecl)]
            public unsafe extern static Int32 Face_GetNormal (IntPtr Handle, out InternalVector3f ANormal);

            [DllImport("gladius.dll", EntryPoint = "gladiuslib_face_getnormala", CallingConvention=CallingConvention.Cdecl)]
            public unsafe extern static Int32 Face_GetNormalA (IntPtr Handle, out InternalVector3f ANormal);

            [DllImport("gladius.dll", EntryPoint = "gladiuslib_face_getnormalb", CallingConvention=CallingConvention.Cdecl)]
            public unsafe extern static Int32 Face_GetNormalB (IntPtr Handle, out InternalVector3f ANormal);

            [DllImport("gladius.dll", EntryPoint = "gladiuslib_face_getnormalc", CallingConvention=CallingConvention.Cdecl)]
            public unsafe extern static Int32 Face_GetNormalC (IntPtr Handle, out InternalVector3f ANormal);

            [DllImport("gladius.dll", EntryPoint = "gladiuslib_detailederroraccessor_getsize", CallingConvention=CallingConvention.Cdecl)]
            public unsafe extern static Int32 DetailedErrorAccessor_GetSize (IntPtr Handle, out UInt64 ASize);

            [DllImport("gladius.dll", EntryPoint = "gladiuslib_detailederroraccessor_next", CallingConvention=CallingConvention.Cdecl)]
            public unsafe extern static Int32 DetailedErrorAccessor_Next (IntPtr Handle, out Byte AResult);

            [DllImport("gladius.dll", EntryPoint = "gladiuslib_detailederroraccessor_prev", CallingConvention=CallingConvention.Cdecl)]
            public unsafe extern static Int32 DetailedErrorAccessor_Prev (IntPtr Handle, out Byte AResult);

            [DllImport("gladius.dll", EntryPoint = "gladiuslib_detailederroraccessor_begin", CallingConvention=CallingConvention.Cdecl)]
            public unsafe extern static Int32 DetailedErrorAccessor_Begin (IntPtr Handle);

            [DllImport("gladius.dll", EntryPoint = "gladiuslib_detailederroraccessor_gettitle", CallingConvention=CallingConvention.Cdecl)]
            public unsafe extern static Int32 DetailedErrorAccessor_GetTitle (IntPtr Handle, UInt32 sizeTitle, out UInt32 neededTitle, IntPtr dataTitle);

            [DllImport("gladius.dll", EntryPoint = "gladiuslib_detailederroraccessor_getdescription", CallingConvention=CallingConvention.Cdecl)]
            public unsafe extern static Int32 DetailedErrorAccessor_GetDescription (IntPtr Handle, UInt32 sizeDescription, out UInt32 neededDescription, IntPtr dataDescription);

            [DllImport("gladius.dll", EntryPoint = "gladiuslib_detailederroraccessor_getseverity", CallingConvention=CallingConvention.Cdecl)]
            public unsafe extern static Int32 DetailedErrorAccessor_GetSeverity (IntPtr Handle, out UInt32 ASeverity);

            [DllImport("gladius.dll", EntryPoint = "gladiuslib_resourceaccessor_getsize", CallingConvention=CallingConvention.Cdecl)]
            public unsafe extern static Int32 ResourceAccessor_GetSize (IntPtr Handle, out UInt64 ASize);

            [DllImport("gladius.dll", EntryPoint = "gladiuslib_resourceaccessor_next", CallingConvention=CallingConvention.Cdecl)]
            public unsafe extern static Int32 ResourceAccessor_Next (IntPtr Handle, out Byte AResult);

            [DllImport("gladius.dll", EntryPoint = "gladiuslib_resourceaccessor_prev", CallingConvention=CallingConvention.Cdecl)]
            public unsafe extern static Int32 ResourceAccessor_Prev (IntPtr Handle, out Byte AResult);

            [DllImport("gladius.dll", EntryPoint = "gladiuslib_resourceaccessor_begin", CallingConvention=CallingConvention.Cdecl)]
            public unsafe extern static Int32 ResourceAccessor_Begin (IntPtr Handle);

            [DllImport("gladius.dll", EntryPoint = "gladiuslib_polygonaccessor_getcurrentvertex", CallingConvention=CallingConvention.Cdecl)]
            public unsafe extern static Int32 PolygonAccessor_GetCurrentVertex (IntPtr Handle, out InternalVector2f AVertex);

            [DllImport("gladius.dll", EntryPoint = "gladiuslib_polygonaccessor_getarea", CallingConvention=CallingConvention.Cdecl)]
            public unsafe extern static Int32 PolygonAccessor_GetArea (IntPtr Handle, out Single AArea);

            [DllImport("gladius.dll", EntryPoint = "gladiuslib_contouraccessor_getcurrentpolygon", CallingConvention=CallingConvention.Cdecl)]
            public unsafe extern static Int32 ContourAccessor_GetCurrentPolygon (IntPtr Handle, out IntPtr AVertex);

            [DllImport("gladius.dll", EntryPoint = "gladiuslib_faceaccessor_getcurrentface", CallingConvention=CallingConvention.Cdecl)]
            public unsafe extern static Int32 FaceAccessor_GetCurrentFace (IntPtr Handle, out IntPtr AFace);

            [DllImport("gladius.dll", EntryPoint = "gladiuslib_channelaccessor_evaluate", CallingConvention=CallingConvention.Cdecl)]
            public unsafe extern static Int32 ChannelAccessor_Evaluate (IntPtr Handle, Single AZ_mm, Single APixelWidth_mm, Single APixelHeight_mm);

            [DllImport("gladius.dll", EntryPoint = "gladiuslib_channelaccessor_getmetainfo", CallingConvention=CallingConvention.Cdecl)]
            public unsafe extern static Int32 ChannelAccessor_GetMetaInfo (IntPtr Handle, out InternalChannelMetaInfo AMetaInfo);

            [DllImport("gladius.dll", EntryPoint = "gladiuslib_channelaccessor_copy", CallingConvention=CallingConvention.Cdecl)]
            public unsafe extern static Int32 ChannelAccessor_Copy (IntPtr Handle, Int64 ATargetPtr);

            [DllImport("gladius.dll", EntryPoint = "gladiuslib_channelaccessor_getname", CallingConvention=CallingConvention.Cdecl)]
            public unsafe extern static Int32 ChannelAccessor_GetName (IntPtr Handle, UInt32 sizeName, out UInt32 neededName, IntPtr dataName);

            [DllImport("gladius.dll", EntryPoint = "gladiuslib_channelaccessor_switchtochannel", CallingConvention=CallingConvention.Cdecl)]
            public unsafe extern static Int32 ChannelAccessor_SwitchToChannel (IntPtr Handle, byte[] AName, out Byte AResult);

            [DllImport("gladius.dll", EntryPoint = "gladiuslib_gladius_loadassembly", CallingConvention=CallingConvention.Cdecl)]
            public unsafe extern static Int32 Gladius_LoadAssembly (IntPtr Handle, byte[] AFilename);

            [DllImport("gladius.dll", EntryPoint = "gladiuslib_gladius_exportstl", CallingConvention=CallingConvention.Cdecl)]
            public unsafe extern static Int32 Gladius_ExportSTL (IntPtr Handle, byte[] AFilename);

            [DllImport("gladius.dll", EntryPoint = "gladiuslib_gladius_getfloatparameter", CallingConvention=CallingConvention.Cdecl)]
            public unsafe extern static Int32 Gladius_GetFloatParameter (IntPtr Handle, byte[] AModelName, byte[] ANodeName, byte[] AParameterName, out Single AValue);

            [DllImport("gladius.dll", EntryPoint = "gladiuslib_gladius_setfloatparameter", CallingConvention=CallingConvention.Cdecl)]
            public unsafe extern static Int32 Gladius_SetFloatParameter (IntPtr Handle, byte[] AModelName, byte[] ANodeName, byte[] AParameterName, Single AValue);

            [DllImport("gladius.dll", EntryPoint = "gladiuslib_gladius_getstringparameter", CallingConvention=CallingConvention.Cdecl)]
            public unsafe extern static Int32 Gladius_GetStringParameter (IntPtr Handle, byte[] AModelName, byte[] ANodeName, byte[] AParameterName, UInt32 sizeValue, out UInt32 neededValue, IntPtr dataValue);

            [DllImport("gladius.dll", EntryPoint = "gladiuslib_gladius_setstringparameter", CallingConvention=CallingConvention.Cdecl)]
            public unsafe extern static Int32 Gladius_SetStringParameter (IntPtr Handle, byte[] AModelName, byte[] ANodeName, byte[] AParameterName, byte[] AValue);

            [DllImport("gladius.dll", EntryPoint = "gladiuslib_gladius_getvector3fparameter", CallingConvention=CallingConvention.Cdecl)]
            public unsafe extern static Int32 Gladius_GetVector3fParameter (IntPtr Handle, byte[] AModelName, byte[] ANodeName, byte[] AParameterName, out InternalVector3f AValue);

            [DllImport("gladius.dll", EntryPoint = "gladiuslib_gladius_setvector3fparameter", CallingConvention=CallingConvention.Cdecl)]
            public unsafe extern static Int32 Gladius_SetVector3fParameter (IntPtr Handle, byte[] AModelName, byte[] ANodeName, byte[] AParameterName, Single AX, Single AY, Single AZ);

            [DllImport("gladius.dll", EntryPoint = "gladiuslib_gladius_generatecontour", CallingConvention=CallingConvention.Cdecl)]
            public unsafe extern static Int32 Gladius_GenerateContour (IntPtr Handle, Single AZ, Single AOffset, out IntPtr AAccessor);

            [DllImport("gladius.dll", EntryPoint = "gladiuslib_gladius_computeboundingbox", CallingConvention=CallingConvention.Cdecl)]
            public unsafe extern static Int32 Gladius_ComputeBoundingBox (IntPtr Handle, out IntPtr ABoundingBox);

            [DllImport("gladius.dll", EntryPoint = "gladiuslib_gladius_generatepreviewmesh", CallingConvention=CallingConvention.Cdecl)]
            public unsafe extern static Int32 Gladius_GeneratePreviewMesh (IntPtr Handle, out IntPtr AFaces);

            [DllImport("gladius.dll", EntryPoint = "gladiuslib_gladius_getchannels", CallingConvention=CallingConvention.Cdecl)]
            public unsafe extern static Int32 Gladius_GetChannels (IntPtr Handle, out IntPtr AAccessor);

            [DllImport("gladius.dll", EntryPoint = "gladiuslib_gladius_getdetailederroraccessor", CallingConvention=CallingConvention.Cdecl)]
            public unsafe extern static Int32 Gladius_GetDetailedErrorAccessor (IntPtr Handle, out IntPtr AAccessor);

            [DllImport("gladius.dll", EntryPoint = "gladiuslib_gladius_cleardetailederrors", CallingConvention=CallingConvention.Cdecl)]
            public unsafe extern static Int32 Gladius_ClearDetailedErrors (IntPtr Handle);

            [DllImport("gladius.dll", EntryPoint = "gladiuslib_gladius_injectsmoothingkernel", CallingConvention=CallingConvention.Cdecl)]
            public unsafe extern static Int32 Gladius_InjectSmoothingKernel (IntPtr Handle, byte[] AKernel);

            [DllImport("gladius.dll", EntryPoint = "gladiuslib_getversion", CharSet = CharSet.Ansi, CallingConvention=CallingConvention.Cdecl)]
            public extern static Int32 GetVersion (out UInt32 AMajor, out UInt32 AMinor, out UInt32 AMicro);

            [DllImport("gladius.dll", EntryPoint = "gladiuslib_getlasterror", CharSet = CharSet.Ansi, CallingConvention=CallingConvention.Cdecl)]
            public extern static Int32 GetLastError (IntPtr AInstance, UInt32 sizeErrorMessage, out UInt32 neededErrorMessage, IntPtr dataErrorMessage, out Byte AHasError);

            [DllImport("gladius.dll", EntryPoint = "gladiuslib_acquireinstance", CharSet = CharSet.Ansi, CallingConvention=CallingConvention.Cdecl)]
            public extern static Int32 AcquireInstance (IntPtr AInstance);

            [DllImport("gladius.dll", EntryPoint = "gladiuslib_releaseinstance", CharSet = CharSet.Ansi, CallingConvention=CallingConvention.Cdecl)]
            public extern static Int32 ReleaseInstance (IntPtr AInstance);

            [DllImport("gladius.dll", EntryPoint = "gladiuslib_creategladius", CharSet = CharSet.Ansi, CallingConvention=CallingConvention.Cdecl)]
            public extern static Int32 CreateGladius (out IntPtr AInstance);

            public unsafe static sVector2f convertInternalToStruct_Vector2f (InternalVector2f intVector2f)
            {
                sVector2f Vector2f;
                Vector2f.Coordinates = new Single[2];
                for (int rowIndex = 0; rowIndex < 2; rowIndex++) {
                    Vector2f.Coordinates[rowIndex] = intVector2f.Coordinates[rowIndex];
                }

                return Vector2f;
            }

            public unsafe static InternalVector2f convertStructToInternal_Vector2f (sVector2f Vector2f)
            {
                InternalVector2f intVector2f;
                for (int rowIndex = 0; rowIndex < 2; rowIndex++) {
                    intVector2f.Coordinates[rowIndex] = Vector2f.Coordinates[rowIndex];
                }

                return intVector2f;
            }

            public unsafe static sVector3f convertInternalToStruct_Vector3f (InternalVector3f intVector3f)
            {
                sVector3f Vector3f;
                Vector3f.Coordinates = new Single[3];
                for (int rowIndex = 0; rowIndex < 3; rowIndex++) {
                    Vector3f.Coordinates[rowIndex] = intVector3f.Coordinates[rowIndex];
                }

                return Vector3f;
            }

            public unsafe static InternalVector3f convertStructToInternal_Vector3f (sVector3f Vector3f)
            {
                InternalVector3f intVector3f;
                for (int rowIndex = 0; rowIndex < 3; rowIndex++) {
                    intVector3f.Coordinates[rowIndex] = Vector3f.Coordinates[rowIndex];
                }

                return intVector3f;
            }

            public unsafe static sChannelMetaInfo convertInternalToStruct_ChannelMetaInfo (InternalChannelMetaInfo intChannelMetaInfo)
            {
                sChannelMetaInfo ChannelMetaInfo;
                ChannelMetaInfo.MinPosition = new Single[2];
                for (int rowIndex = 0; rowIndex < 2; rowIndex++) {
                    ChannelMetaInfo.MinPosition[rowIndex] = intChannelMetaInfo.MinPosition[rowIndex];
                }

                ChannelMetaInfo.MaxPosition = new Single[2];
                for (int rowIndex = 0; rowIndex < 2; rowIndex++) {
                    ChannelMetaInfo.MaxPosition[rowIndex] = intChannelMetaInfo.MaxPosition[rowIndex];
                }

                ChannelMetaInfo.Size = new Int32[2];
                for (int rowIndex = 0; rowIndex < 2; rowIndex++) {
                    ChannelMetaInfo.Size[rowIndex] = intChannelMetaInfo.Size[rowIndex];
                }

                ChannelMetaInfo.RequiredMemory = intChannelMetaInfo.RequiredMemory;
                return ChannelMetaInfo;
            }

            public unsafe static InternalChannelMetaInfo convertStructToInternal_ChannelMetaInfo (sChannelMetaInfo ChannelMetaInfo)
            {
                InternalChannelMetaInfo intChannelMetaInfo;
                for (int rowIndex = 0; rowIndex < 2; rowIndex++) {
                    intChannelMetaInfo.MinPosition[rowIndex] = ChannelMetaInfo.MinPosition[rowIndex];
                }

                for (int rowIndex = 0; rowIndex < 2; rowIndex++) {
                    intChannelMetaInfo.MaxPosition[rowIndex] = ChannelMetaInfo.MaxPosition[rowIndex];
                }

                for (int rowIndex = 0; rowIndex < 2; rowIndex++) {
                    intChannelMetaInfo.Size[rowIndex] = ChannelMetaInfo.Size[rowIndex];
                }

                intChannelMetaInfo.RequiredMemory = ChannelMetaInfo.RequiredMemory;
                return intChannelMetaInfo;
            }

            public static void ThrowError(IntPtr Handle, Int32 errorCode)
            {
                String sMessage = "GladiusLib Error";
                if (Handle != IntPtr.Zero) {
                    UInt32 sizeMessage = 0;
                    UInt32 neededMessage = 0;
                    Byte hasLastError = 0;
                    Int32 resultCode1 = GetLastError (Handle, sizeMessage, out neededMessage, IntPtr.Zero, out hasLastError);
                    if ((resultCode1 == 0) && (hasLastError != 0)) {
                        sizeMessage = neededMessage;
                        byte[] bytesMessage = new byte[sizeMessage];

                        GCHandle dataMessage = GCHandle.Alloc(bytesMessage, GCHandleType.Pinned);
                        Int32 resultCode2 = GetLastError(Handle, sizeMessage, out neededMessage, dataMessage.AddrOfPinnedObject(), out hasLastError);
                        dataMessage.Free();

                        if ((resultCode2 == 0) && (hasLastError != 0)) {
                            sMessage = sMessage + ": " + Encoding.UTF8.GetString(bytesMessage).TrimEnd(char.MinValue);
                        }
                    }
                }

                throw new Exception(sMessage + "(# " + errorCode + ")");
            }

        }
    }


    class CBase 
    {
        protected IntPtr Handle;

        public CBase (IntPtr NewHandle)
        {
            Handle = NewHandle;
        }

        ~CBase ()
        {
            if (Handle != IntPtr.Zero) {
                Internal.GladiusLibWrapper.ReleaseInstance (Handle);
                Handle = IntPtr.Zero;
            }
        }

        protected void CheckError (Int32 errorCode)
        {
            if (errorCode != 0) {
                Internal.GladiusLibWrapper.ThrowError (Handle, errorCode);
            }
        }

        public IntPtr GetHandle ()
        {
            return Handle;
        }

    }

    class CBoundingBox : CBase
    {
        public CBoundingBox (IntPtr NewHandle) : base (NewHandle)
        {
        }

        public sVector3f GetMin ()
        {
            Internal.InternalVector3f intresultMin;

            CheckError(Internal.GladiusLibWrapper.BoundingBox_GetMin (Handle, out intresultMin));
            return Internal.GladiusLibWrapper.convertInternalToStruct_Vector3f (intresultMin);
        }

        public sVector3f GetMax ()
        {
            Internal.InternalVector3f intresultMax;

            CheckError(Internal.GladiusLibWrapper.BoundingBox_GetMax (Handle, out intresultMax));
            return Internal.GladiusLibWrapper.convertInternalToStruct_Vector3f (intresultMax);
        }

    }

    class CFace : CBase
    {
        public CFace (IntPtr NewHandle) : base (NewHandle)
        {
        }

        public sVector3f GetVertexA ()
        {
            Internal.InternalVector3f intresultVertexA;

            CheckError(Internal.GladiusLibWrapper.Face_GetVertexA (Handle, out intresultVertexA));
            return Internal.GladiusLibWrapper.convertInternalToStruct_Vector3f (intresultVertexA);
        }

        public sVector3f GetVertexB ()
        {
            Internal.InternalVector3f intresultVertexB;

            CheckError(Internal.GladiusLibWrapper.Face_GetVertexB (Handle, out intresultVertexB));
            return Internal.GladiusLibWrapper.convertInternalToStruct_Vector3f (intresultVertexB);
        }

        public sVector3f GetVertexC ()
        {
            Internal.InternalVector3f intresultVertexC;

            CheckError(Internal.GladiusLibWrapper.Face_GetVertexC (Handle, out intresultVertexC));
            return Internal.GladiusLibWrapper.convertInternalToStruct_Vector3f (intresultVertexC);
        }

        public sVector3f GetNormal ()
        {
            Internal.InternalVector3f intresultNormal;

            CheckError(Internal.GladiusLibWrapper.Face_GetNormal (Handle, out intresultNormal));
            return Internal.GladiusLibWrapper.convertInternalToStruct_Vector3f (intresultNormal);
        }

        public sVector3f GetNormalA ()
        {
            Internal.InternalVector3f intresultNormal;

            CheckError(Internal.GladiusLibWrapper.Face_GetNormalA (Handle, out intresultNormal));
            return Internal.GladiusLibWrapper.convertInternalToStruct_Vector3f (intresultNormal);
        }

        public sVector3f GetNormalB ()
        {
            Internal.InternalVector3f intresultNormal;

            CheckError(Internal.GladiusLibWrapper.Face_GetNormalB (Handle, out intresultNormal));
            return Internal.GladiusLibWrapper.convertInternalToStruct_Vector3f (intresultNormal);
        }

        public sVector3f GetNormalC ()
        {
            Internal.InternalVector3f intresultNormal;

            CheckError(Internal.GladiusLibWrapper.Face_GetNormalC (Handle, out intresultNormal));
            return Internal.GladiusLibWrapper.convertInternalToStruct_Vector3f (intresultNormal);
        }

    }

    class CDetailedErrorAccessor : CBase
    {
        public CDetailedErrorAccessor (IntPtr NewHandle) : base (NewHandle)
        {
        }

        public UInt64 GetSize ()
        {
            UInt64 resultSize = 0;

            CheckError(Internal.GladiusLibWrapper.DetailedErrorAccessor_GetSize (Handle, out resultSize));
            return resultSize;
        }

        public bool Next ()
        {
            Byte resultResult = 0;

            CheckError(Internal.GladiusLibWrapper.DetailedErrorAccessor_Next (Handle, out resultResult));
            return (resultResult != 0);
        }

        public bool Prev ()
        {
            Byte resultResult = 0;

            CheckError(Internal.GladiusLibWrapper.DetailedErrorAccessor_Prev (Handle, out resultResult));
            return (resultResult != 0);
        }

        public void Begin ()
        {

            CheckError(Internal.GladiusLibWrapper.DetailedErrorAccessor_Begin (Handle));
        }

        public String GetTitle ()
        {
            UInt32 sizeTitle = 0;
            UInt32 neededTitle = 0;
            CheckError(Internal.GladiusLibWrapper.DetailedErrorAccessor_GetTitle (Handle, sizeTitle, out neededTitle, IntPtr.Zero));
            sizeTitle = neededTitle;
            byte[] bytesTitle = new byte[sizeTitle];
            GCHandle dataTitle = GCHandle.Alloc(bytesTitle, GCHandleType.Pinned);

            CheckError(Internal.GladiusLibWrapper.DetailedErrorAccessor_GetTitle (Handle, sizeTitle, out neededTitle, dataTitle.AddrOfPinnedObject()));
            dataTitle.Free();
            return Encoding.UTF8.GetString(bytesTitle).TrimEnd(char.MinValue);
        }

        public String GetDescription ()
        {
            UInt32 sizeDescription = 0;
            UInt32 neededDescription = 0;
            CheckError(Internal.GladiusLibWrapper.DetailedErrorAccessor_GetDescription (Handle, sizeDescription, out neededDescription, IntPtr.Zero));
            sizeDescription = neededDescription;
            byte[] bytesDescription = new byte[sizeDescription];
            GCHandle dataDescription = GCHandle.Alloc(bytesDescription, GCHandleType.Pinned);

            CheckError(Internal.GladiusLibWrapper.DetailedErrorAccessor_GetDescription (Handle, sizeDescription, out neededDescription, dataDescription.AddrOfPinnedObject()));
            dataDescription.Free();
            return Encoding.UTF8.GetString(bytesDescription).TrimEnd(char.MinValue);
        }

        public UInt32 GetSeverity ()
        {
            UInt32 resultSeverity = 0;

            CheckError(Internal.GladiusLibWrapper.DetailedErrorAccessor_GetSeverity (Handle, out resultSeverity));
            return resultSeverity;
        }

    }

    class CResourceAccessor : CBase
    {
        public CResourceAccessor (IntPtr NewHandle) : base (NewHandle)
        {
        }

        public UInt64 GetSize ()
        {
            UInt64 resultSize = 0;

            CheckError(Internal.GladiusLibWrapper.ResourceAccessor_GetSize (Handle, out resultSize));
            return resultSize;
        }

        public bool Next ()
        {
            Byte resultResult = 0;

            CheckError(Internal.GladiusLibWrapper.ResourceAccessor_Next (Handle, out resultResult));
            return (resultResult != 0);
        }

        public bool Prev ()
        {
            Byte resultResult = 0;

            CheckError(Internal.GladiusLibWrapper.ResourceAccessor_Prev (Handle, out resultResult));
            return (resultResult != 0);
        }

        public void Begin ()
        {

            CheckError(Internal.GladiusLibWrapper.ResourceAccessor_Begin (Handle));
        }

    }

    class CPolygonAccessor : CResourceAccessor
    {
        public CPolygonAccessor (IntPtr NewHandle) : base (NewHandle)
        {
        }

        public sVector2f GetCurrentVertex ()
        {
            Internal.InternalVector2f intresultVertex;

            CheckError(Internal.GladiusLibWrapper.PolygonAccessor_GetCurrentVertex (Handle, out intresultVertex));
            return Internal.GladiusLibWrapper.convertInternalToStruct_Vector2f (intresultVertex);
        }

        public Single GetArea ()
        {
            Single resultArea = 0;

            CheckError(Internal.GladiusLibWrapper.PolygonAccessor_GetArea (Handle, out resultArea));
            return resultArea;
        }

    }

    class CContourAccessor : CResourceAccessor
    {
        public CContourAccessor (IntPtr NewHandle) : base (NewHandle)
        {
        }

        public CPolygonAccessor GetCurrentPolygon ()
        {
            IntPtr newVertex = IntPtr.Zero;

            CheckError(Internal.GladiusLibWrapper.ContourAccessor_GetCurrentPolygon (Handle, out newVertex));
            return new CPolygonAccessor (newVertex );
        }

    }

    class CFaceAccessor : CResourceAccessor
    {
        public CFaceAccessor (IntPtr NewHandle) : base (NewHandle)
        {
        }

        public CFace GetCurrentFace ()
        {
            IntPtr newFace = IntPtr.Zero;

            CheckError(Internal.GladiusLibWrapper.FaceAccessor_GetCurrentFace (Handle, out newFace));
            return new CFace (newFace );
        }

    }

    class CChannelAccessor : CResourceAccessor
    {
        public CChannelAccessor (IntPtr NewHandle) : base (NewHandle)
        {
        }

        public void Evaluate (Single AZ_mm, Single APixelWidth_mm, Single APixelHeight_mm)
        {

            CheckError(Internal.GladiusLibWrapper.ChannelAccessor_Evaluate (Handle, AZ_mm, APixelWidth_mm, APixelHeight_mm));
        }

        public sChannelMetaInfo GetMetaInfo ()
        {
            Internal.InternalChannelMetaInfo intresultMetaInfo;

            CheckError(Internal.GladiusLibWrapper.ChannelAccessor_GetMetaInfo (Handle, out intresultMetaInfo));
            return Internal.GladiusLibWrapper.convertInternalToStruct_ChannelMetaInfo (intresultMetaInfo);
        }

        public void Copy (Int64 ATargetPtr)
        {

            CheckError(Internal.GladiusLibWrapper.ChannelAccessor_Copy (Handle, ATargetPtr));
        }

        public String GetName ()
        {
            UInt32 sizeName = 0;
            UInt32 neededName = 0;
            CheckError(Internal.GladiusLibWrapper.ChannelAccessor_GetName (Handle, sizeName, out neededName, IntPtr.Zero));
            sizeName = neededName;
            byte[] bytesName = new byte[sizeName];
            GCHandle dataName = GCHandle.Alloc(bytesName, GCHandleType.Pinned);

            CheckError(Internal.GladiusLibWrapper.ChannelAccessor_GetName (Handle, sizeName, out neededName, dataName.AddrOfPinnedObject()));
            dataName.Free();
            return Encoding.UTF8.GetString(bytesName).TrimEnd(char.MinValue);
        }

        public bool SwitchToChannel (String AName)
        {
            byte[] byteName = Encoding.UTF8.GetBytes(AName + char.MinValue);
            Byte resultResult = 0;

            CheckError(Internal.GladiusLibWrapper.ChannelAccessor_SwitchToChannel (Handle, byteName, out resultResult));
            return (resultResult != 0);
        }

    }

    class CGladius : CBase
    {
        public CGladius (IntPtr NewHandle) : base (NewHandle)
        {
        }

        public void LoadAssembly (String AFilename)
        {
            byte[] byteFilename = Encoding.UTF8.GetBytes(AFilename + char.MinValue);

            CheckError(Internal.GladiusLibWrapper.Gladius_LoadAssembly (Handle, byteFilename));
        }

        public void ExportSTL (String AFilename)
        {
            byte[] byteFilename = Encoding.UTF8.GetBytes(AFilename + char.MinValue);

            CheckError(Internal.GladiusLibWrapper.Gladius_ExportSTL (Handle, byteFilename));
        }

        public Single GetFloatParameter (String AModelName, String ANodeName, String AParameterName)
        {
            byte[] byteModelName = Encoding.UTF8.GetBytes(AModelName + char.MinValue);
            byte[] byteNodeName = Encoding.UTF8.GetBytes(ANodeName + char.MinValue);
            byte[] byteParameterName = Encoding.UTF8.GetBytes(AParameterName + char.MinValue);
            Single resultValue = 0;

            CheckError(Internal.GladiusLibWrapper.Gladius_GetFloatParameter (Handle, byteModelName, byteNodeName, byteParameterName, out resultValue));
            return resultValue;
        }

        public void SetFloatParameter (String AModelName, String ANodeName, String AParameterName, Single AValue)
        {
            byte[] byteModelName = Encoding.UTF8.GetBytes(AModelName + char.MinValue);
            byte[] byteNodeName = Encoding.UTF8.GetBytes(ANodeName + char.MinValue);
            byte[] byteParameterName = Encoding.UTF8.GetBytes(AParameterName + char.MinValue);

            CheckError(Internal.GladiusLibWrapper.Gladius_SetFloatParameter (Handle, byteModelName, byteNodeName, byteParameterName, AValue));
        }

        public String GetStringParameter (String AModelName, String ANodeName, String AParameterName)
        {
            byte[] byteModelName = Encoding.UTF8.GetBytes(AModelName + char.MinValue);
            byte[] byteNodeName = Encoding.UTF8.GetBytes(ANodeName + char.MinValue);
            byte[] byteParameterName = Encoding.UTF8.GetBytes(AParameterName + char.MinValue);
            UInt32 sizeValue = 0;
            UInt32 neededValue = 0;
            CheckError(Internal.GladiusLibWrapper.Gladius_GetStringParameter (Handle, byteModelName, byteNodeName, byteParameterName, sizeValue, out neededValue, IntPtr.Zero));
            sizeValue = neededValue;
            byte[] bytesValue = new byte[sizeValue];
            GCHandle dataValue = GCHandle.Alloc(bytesValue, GCHandleType.Pinned);

            CheckError(Internal.GladiusLibWrapper.Gladius_GetStringParameter (Handle, byteModelName, byteNodeName, byteParameterName, sizeValue, out neededValue, dataValue.AddrOfPinnedObject()));
            dataValue.Free();
            return Encoding.UTF8.GetString(bytesValue).TrimEnd(char.MinValue);
        }

        public void SetStringParameter (String AModelName, String ANodeName, String AParameterName, String AValue)
        {
            byte[] byteModelName = Encoding.UTF8.GetBytes(AModelName + char.MinValue);
            byte[] byteNodeName = Encoding.UTF8.GetBytes(ANodeName + char.MinValue);
            byte[] byteParameterName = Encoding.UTF8.GetBytes(AParameterName + char.MinValue);
            byte[] byteValue = Encoding.UTF8.GetBytes(AValue + char.MinValue);

            CheckError(Internal.GladiusLibWrapper.Gladius_SetStringParameter (Handle, byteModelName, byteNodeName, byteParameterName, byteValue));
        }

        public sVector3f GetVector3fParameter (String AModelName, String ANodeName, String AParameterName)
        {
            byte[] byteModelName = Encoding.UTF8.GetBytes(AModelName + char.MinValue);
            byte[] byteNodeName = Encoding.UTF8.GetBytes(ANodeName + char.MinValue);
            byte[] byteParameterName = Encoding.UTF8.GetBytes(AParameterName + char.MinValue);
            Internal.InternalVector3f intresultValue;

            CheckError(Internal.GladiusLibWrapper.Gladius_GetVector3fParameter (Handle, byteModelName, byteNodeName, byteParameterName, out intresultValue));
            return Internal.GladiusLibWrapper.convertInternalToStruct_Vector3f (intresultValue);
        }

        public void SetVector3fParameter (String AModelName, String ANodeName, String AParameterName, Single AX, Single AY, Single AZ)
        {
            byte[] byteModelName = Encoding.UTF8.GetBytes(AModelName + char.MinValue);
            byte[] byteNodeName = Encoding.UTF8.GetBytes(ANodeName + char.MinValue);
            byte[] byteParameterName = Encoding.UTF8.GetBytes(AParameterName + char.MinValue);

            CheckError(Internal.GladiusLibWrapper.Gladius_SetVector3fParameter (Handle, byteModelName, byteNodeName, byteParameterName, AX, AY, AZ));
        }

        public CContourAccessor GenerateContour (Single AZ, Single AOffset)
        {
            IntPtr newAccessor = IntPtr.Zero;

            CheckError(Internal.GladiusLibWrapper.Gladius_GenerateContour (Handle, AZ, AOffset, out newAccessor));
            return new CContourAccessor (newAccessor );
        }

        public CBoundingBox ComputeBoundingBox ()
        {
            IntPtr newBoundingBox = IntPtr.Zero;

            CheckError(Internal.GladiusLibWrapper.Gladius_ComputeBoundingBox (Handle, out newBoundingBox));
            return new CBoundingBox (newBoundingBox );
        }

        public CFaceAccessor GeneratePreviewMesh ()
        {
            IntPtr newFaces = IntPtr.Zero;

            CheckError(Internal.GladiusLibWrapper.Gladius_GeneratePreviewMesh (Handle, out newFaces));
            return new CFaceAccessor (newFaces );
        }

        public CChannelAccessor GetChannels ()
        {
            IntPtr newAccessor = IntPtr.Zero;

            CheckError(Internal.GladiusLibWrapper.Gladius_GetChannels (Handle, out newAccessor));
            return new CChannelAccessor (newAccessor );
        }

        public CDetailedErrorAccessor GetDetailedErrorAccessor ()
        {
            IntPtr newAccessor = IntPtr.Zero;

            CheckError(Internal.GladiusLibWrapper.Gladius_GetDetailedErrorAccessor (Handle, out newAccessor));
            return new CDetailedErrorAccessor (newAccessor );
        }

        public void ClearDetailedErrors ()
        {

            CheckError(Internal.GladiusLibWrapper.Gladius_ClearDetailedErrors (Handle));
        }

        public void InjectSmoothingKernel (String AKernel)
        {
            byte[] byteKernel = Encoding.UTF8.GetBytes(AKernel + char.MinValue);

            CheckError(Internal.GladiusLibWrapper.Gladius_InjectSmoothingKernel (Handle, byteKernel));
        }

    }

    class Wrapper
    {
        private static void CheckError (Int32 errorCode)
        {
            if (errorCode != 0) {
                Internal.GladiusLibWrapper.ThrowError (IntPtr.Zero, errorCode);
            }
        }

        public static void GetVersion (out UInt32 AMajor, out UInt32 AMinor, out UInt32 AMicro)
        {

            CheckError(Internal.GladiusLibWrapper.GetVersion (out AMajor, out AMinor, out AMicro));
        }

        public static bool GetLastError (CBase AInstance, out String AErrorMessage)
        {
            Byte resultHasError = 0;
            UInt32 sizeErrorMessage = 0;
            UInt32 neededErrorMessage = 0;
            CheckError(Internal.GladiusLibWrapper.GetLastError (AInstance.GetHandle(), sizeErrorMessage, out neededErrorMessage, IntPtr.Zero, out resultHasError));
            sizeErrorMessage = neededErrorMessage;
            byte[] bytesErrorMessage = new byte[sizeErrorMessage];
            GCHandle dataErrorMessage = GCHandle.Alloc(bytesErrorMessage, GCHandleType.Pinned);

            CheckError(Internal.GladiusLibWrapper.GetLastError (AInstance.GetHandle(), sizeErrorMessage, out neededErrorMessage, dataErrorMessage.AddrOfPinnedObject(), out resultHasError));
            dataErrorMessage.Free();
            AErrorMessage = Encoding.UTF8.GetString(bytesErrorMessage).TrimEnd(char.MinValue);
            return (resultHasError != 0);
        }

        public static void AcquireInstance (CBase AInstance)
        {

            CheckError(Internal.GladiusLibWrapper.AcquireInstance (AInstance.GetHandle()));
        }

        public static void ReleaseInstance (CBase AInstance)
        {

            CheckError(Internal.GladiusLibWrapper.ReleaseInstance (AInstance.GetHandle()));
        }

        public static CGladius CreateGladius ()
        {
            IntPtr newInstance = IntPtr.Zero;

            CheckError(Internal.GladiusLibWrapper.CreateGladius (out newInstance));
            return new CGladius (newInstance );
        }

    }

}
