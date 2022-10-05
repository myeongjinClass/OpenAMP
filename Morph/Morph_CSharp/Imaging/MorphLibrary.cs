//--------------------------------------------------------------------------
// 
//  Copyright (c) Microsoft Corporation.  All rights reserved. 
// 
//  File: MorphLibrary.cs
//
//--------------------------------------------------------------------------

using System;
using System.Collections;
using System.Runtime.InteropServices;
using System.Text;

namespace ParallelMorph
{
    public class MorphLibrary
    {
        public enum Mode : int
        {
            Sequential = 0,
            PPL = 1,
            CppAMP = 2
        }

        [DllImport("Morph_lib", CallingConvention = CallingConvention.StdCall)]
        public extern static uint BeginEnumAccelerators(out IntPtr handle);

        [DllImport("Morph_lib", CallingConvention = CallingConvention.StdCall, CharSet = CharSet.Unicode)]
        [return: MarshalAs(UnmanagedType.U1)]
        public extern static bool EnumAccelerator(IntPtr handle, uint index, StringBuilder description, uint descriptionCapacity, StringBuilder devicePath, uint devicePathCapacity);

        [DllImport("Morph_lib", CallingConvention = CallingConvention.StdCall)]
        public extern static void EndEnumAccelerators(IntPtr handle);

        [DllImport("Morph_lib", CallingConvention = CallingConvention.StdCall, CharSet = CharSet.Unicode)]
        [return: MarshalAs(UnmanagedType.U1)]
        public extern unsafe static bool InitializeMorph(Mode mode, string ampDevicePath, byte* startImage, uint startImageBytesInARow, byte* endImage, uint endImageBytesInARow,
            int width, int height, float* linePairsDataPtr, uint linePairsNum, double constA, double constB, double constP);

        [DllImport("Morph_lib", CallingConvention = CallingConvention.StdCall)]
        public extern unsafe static void ShutdownMorph();

        [DllImport("Morph_lib", CallingConvention = CallingConvention.StdCall)]
        [return: MarshalAs(UnmanagedType.U1)]
        public extern unsafe static bool ComputeMorphStep(byte* output, double percentage);

        public static float[] ConvertToFloatArray(LinePairCollection pairs)
        {
            float[] pairsData = new float[pairs.Count * 2 * 4];
            uint idx = 0;
            foreach (var pair in pairs)
            {
                pairsData[idx++] = pair.Item1.Item1.X;
                pairsData[idx++] = pair.Item1.Item1.Y;
                pairsData[idx++] = pair.Item1.Item2.X;
                pairsData[idx++] = pair.Item1.Item2.Y;
                pairsData[idx++] = pair.Item2.Item1.X;
                pairsData[idx++] = pair.Item2.Item1.Y;
                pairsData[idx++] = pair.Item2.Item2.X;
                pairsData[idx++] = pair.Item2.Item2.Y;
            }
            return pairsData;
        }
        
        public class Accelerator
        {
            private string _description;
            private string _devicePath;

            public Accelerator(string description, string devicePath)
            {
                _description = description;
                _devicePath = devicePath;
            }

            public string Description { get { return _description; } }

            public string DevicePath { get { return _devicePath; } }
        }

        public static Accelerator[] ListAccelerators()
        {
            IntPtr handle;
            uint numAccelerators = BeginEnumAccelerators(out handle);
            try
            {
                Accelerator[] ret = new Accelerator[numAccelerators];
                for (uint i = 0; i < numAccelerators; i++)
                {
                    StringBuilder description = new StringBuilder(255);
                    StringBuilder devicePath = new StringBuilder(255);
                    if (!MorphLibrary.EnumAccelerator(handle, i, description, (uint)description.Capacity, devicePath, (uint)devicePath.Capacity))
                    {
                        throw new Exception("EnumAccelerator has failed!");
                    }
                    ret[i] = new Accelerator(description.ToString(), devicePath.ToString());
                }
                return ret;
            }
            finally
            {
                MorphLibrary.EndEnumAccelerators(handle);
            }
        }
    }
}