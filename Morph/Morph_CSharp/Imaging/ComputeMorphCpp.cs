//--------------------------------------------------------------------------
// 
//  Copyright (c) Microsoft Corporation.  All rights reserved. 
// 
//  File: ComputeMorphCpp.cs
//
//--------------------------------------------------------------------------

using System;
using System.ComponentModel;
using System.Drawing;
using System.Threading;
using System.Threading.Tasks;
using Microsoft.Drawing;

namespace ParallelMorph
{
    public sealed class ComputeMorphCpp : ComputeMorph
    {
        class MorphLibraryGuard : IDisposable
        {
            bool initialized = false;

            public unsafe MorphLibraryGuard(MorphLibrary.Mode mode, MorphLibrary.Accelerator accelerator, FastBitmap startImage, FastBitmap endImage, LinePairCollection lines, ComputeMorphOptions options)
            {
                if (startImage.Size != endImage.Size)
                {
                    throw new ArgumentException("Start and end images have to have the same size!");
                }

                fixed (float* linePairsDataPtr = MorphLibrary.ConvertToFloatArray(lines))
                {
                    initialized = MorphLibrary.InitializeMorph(mode, accelerator != null ? accelerator.DevicePath : "", startImage.Data, (uint)startImage.BytesInARow, endImage.Data,
                        (uint)endImage.BytesInARow, startImage.Size.Width, startImage.Size.Height,
                        linePairsDataPtr, (uint)lines.Count, options.ConstA, options.ConstB, options.ConstP);
                }

                if (!initialized)
                {
                    throw new Exception("Failed to initialize MorphLibrary!");
                }
            }

            public void Dispose()
            {
                if (initialized)
                    MorphLibrary.ShutdownMorph();
            }
        }

        MorphLibrary.Mode _mode;
        MorphLibrary.Accelerator _accelerator;

        public ComputeMorphCpp(ComputeMorphOptions options, MorphLibrary.Mode mode, MorphLibrary.Accelerator accelerator, CancellationToken cancellationToken)
            : base(options, cancellationToken)
        {
            _mode = mode;
            _accelerator = accelerator;
        }

        protected override unsafe Bitmap RenderFrame(LinePairCollection lines, FastBitmap startImage, FastBitmap endImage, double percent)
        {
            Bitmap output = new Bitmap(startImage.Size.Width, startImage.Size.Height);
            using (FastBitmap fastOut = new FastBitmap(output))
                MorphLibrary.ComputeMorphStep(fastOut.Data, percent);
            return output;
        }

        override protected IDisposable CreateState(FastBitmap startImage, FastBitmap endImage, LinePairCollection lines, ComputeMorphOptions options)
        {
            return new MorphLibraryGuard(_mode, _accelerator, startImage, endImage, lines, _options);
        }
    }
}