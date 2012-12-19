#include <cstdio>
#include <cstring>
#include <cmath>
#include <float.h>
#include "normalize.h"
#ifdef _WIN32
#include "win32util.h"
#endif

Normalizer::Normalizer(const x::shared_ptr<ISource> &src)
    : DelegatingSource(src), m_peak(0.0), m_processed(0), m_samples_read(0)
{
    const SampleFormat &srcFormat = source()->getSampleFormat();
    if (srcFormat.m_bitsPerSample == 64)
        throw std::runtime_error("Can't handle 64bit sample");

    m_format = SampleFormat("F32LE", srcFormat.m_nchannels, srcFormat.m_rate);

#if defined(_MSC_VER) || defined(__MINGW32__)
    FILE *tmpfile = win32_tmpfile(L"qaac.norm");
#else
    FILE *tmpfile = std::tmpfile();
#endif
    m_tmpfile = x::shared_ptr<FILE>(tmpfile, std::fclose);
}

size_t Normalizer::process(size_t nsamples)
{
    size_t nc = readSamplesAsFloat(source(), &m_ibuffer, &m_fbuffer, nsamples);
    if (nc > 0) {
        m_processed += nc;
        std::fwrite(&m_fbuffer[0], sizeof(float), nc * m_format.m_nchannels,
                    m_tmpfile.get());
        if (std::ferror(m_tmpfile.get()))
            throw_crt_error("fwrite()");
        for (size_t i = 0; i < m_fbuffer.size(); ++i) {
            float x = std::abs(m_fbuffer[i]);
            if (x > m_peak) m_peak = x;
        }
    } else
        std::fseek(m_tmpfile.get(), 0, SEEK_SET);
    return nc;
}

size_t Normalizer::readSamples(void *buffer, size_t nsamples)
{
    size_t nc = std::fread(buffer, sizeof(float),
                           nsamples * m_format.m_nchannels, m_tmpfile.get());
    float *fp = reinterpret_cast<float*>(buffer);
    if (m_peak > 1.0 || (m_peak > FLT_EPSILON && m_peak < 1.0 - FLT_EPSILON)) {
        for (size_t i = 0; i < nc; ++i) {
            float nfp = static_cast<float>(*fp / m_peak);
            *fp++ = nfp;
        }
    }
    nsamples = nc / m_format.m_nchannels;
    m_samples_read += nsamples;
    return nsamples;
}
