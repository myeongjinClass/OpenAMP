//--------------------------------------------------------------------------
// 
//  Copyright (c) Microsoft Corporation.  All rights reserved. 
// 
//  File: ComputeMorph.cs
//
//--------------------------------------------------------------------------

using System;
using System.ComponentModel;
using System.Drawing;
using System.Runtime.InteropServices;
using System.Threading;
using System.Threading.Tasks;
using Microsoft.Drawing;

namespace ParallelMorph
{
    /// <summary>Computes a morph between two images.</summary>
    abstract public class ComputeMorph
    {
        protected ComputeMorphOptions _options;
        protected int _curFrame = 0;
        protected CancellationToken _cancellationToken;

        protected ComputeMorph(ComputeMorphOptions options, CancellationToken cancellationToken)
        {
            if (options == null) throw new ArgumentNullException("options");
            _options = options;
            _cancellationToken = cancellationToken;
        }

        public event ProgressChangedEventHandler ProgressChanged;

        protected void UpdateProgressChanged()
        {
            ProgressChangedEventHandler handler = ProgressChanged;
            if (handler != null)
            {
                double progress = _curFrame * 100.0 / _options.NumberOfOutputFrames;
                handler(this, new ProgressChangedEventArgs((int)progress, null));
            }
        }

        abstract protected Bitmap RenderFrame(LinePairCollection lines, FastBitmap startImage, FastBitmap endImage, double percent);

        /// <summary>
        /// Allows a derived class to initialize common state to be saved between calls to RenderFrame for a single call to Render.
        /// </summary>
        virtual protected IDisposable CreateState(FastBitmap startImage, FastBitmap endImage, LinePairCollection lines, ComputeMorphOptions options)
        {
            return null;
        }

        /// <summary>Create the morphed images and video.</summary>
        public void Render(IImageWriter imageWriter, LinePairCollection lines, Bitmap startImageBmp, Bitmap endImageBmp)
        {
            _curFrame = 0;
            double percentagePerFrame = 1.0 / (_options.NumberOfOutputFrames - 1);

            using (Bitmap clonedStart = Utilities.CreateNewBitmapFrom(startImageBmp))
            using (Bitmap clonedEnd = Utilities.CreateNewBitmapFrom(endImageBmp))
            {
                // Write out the starting picture
                imageWriter.AddFrame(clonedStart);
                _curFrame++;
                UpdateProgressChanged();

                // Generate and write intermediate pictures
                using (FastBitmap startImage = new FastBitmap(startImageBmp))
                using (FastBitmap endImage = new FastBitmap(endImageBmp))
                using (IDisposable state = CreateState(startImage, endImage, lines, _options))
                {
                    for (int i = 1; i < _options.NumberOfOutputFrames - 1; i++)
                    {
                        _cancellationToken.ThrowIfCancellationRequested();
                        using (Bitmap frame = RenderFrame(lines, startImage, endImage, percentagePerFrame * i))
                        {
                            imageWriter.AddFrame(frame);
                        }
                        _curFrame++;
                        UpdateProgressChanged();
                    }
                }

                // Write out the ending picture
                imageWriter.AddFrame(clonedEnd);
                _curFrame++;
                UpdateProgressChanged();
            }
        }
    }
}