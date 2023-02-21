#include "conv2d_eigen.h"

namespace RTNeural
{
template <typename T>
Conv2D<T>::Conv2D(int in_num_filters_in, int in_num_filters_out, int in_num_features_in, int in_kernel_size_time, int in_kernel_size_feature, int in_dilation_rate, int in_stride)
    : num_filters_in(in_num_filters_in)
    , num_filters_out(in_num_filters_out)
    , num_features_in(in_num_features_in)
    , kernel_size_time(in_kernel_size_time)
    , kernel_size_feature(in_kernel_size_feature)
    , dilation_rate(in_dilation_rate)
    , stride(in_stride)
    , num_features_out((in_num_features_in - in_kernel_size_feature) / in_stride + 1)
    , receptive_field(1 + (in_kernel_size_time - 1) * in_dilation_rate)
    , Layer<T>(in_num_features_in * in_num_filters_in, ((in_num_features_in - in_kernel_size_feature) / in_stride + 1) * in_num_filters_out)
{
    bias = Eigen::Vector<T, Eigen::Dynamic>::Zero(num_filters_out);

    for(int i = 0; i < receptive_field; i++)
        state.push_back(Eigen::Matrix<T, Eigen::Dynamic, Eigen::Dynamic>::Zero(num_filters_out, num_features_out));
}

template <typename T>
Conv2D<T>::Conv2D(std::initializer_list<int> sizes)
    : Conv2D<T>(*sizes.begin(), *(sizes.begin() + 1), *(sizes.begin() + 2), *(sizes.begin() + 3), *(sizes.begin() + 4), *(sizes.begin() + 5), *(sizes.begin() + 6))
{
}

template <typename T>
Conv2D<T>::Conv2D(const Conv2D& other)
    : Conv2D<T>(other.num_filters_in, other.num_filters_out, other.num_features_in, other.kernel_size_time, other.kernel_size_feature, other.dilation_rate, other.stride)
{
}

template <typename T>
Conv2D<T>& Conv2D<T>::operator=(const Conv2D& other)
{
    return *this = Conv2D<T>(other);
}

template <typename T>
void Conv2D<T>::setWeights(const std::vector<std::vector<std::vector<std::vector<T>>>>& inWeights)
{
    conv1dLayers.clear();
    for(int i = 0; i < kernel_size_time; i++)
    {
        conv1dLayers.push_back(Conv1DStateless<T, false>(num_filters_in, num_features_in, num_filters_out, kernel_size_feature, stride));
        conv1dLayers[i].setWeights(inWeights[i]);
    }
}

template <typename T>
void Conv2D<T>::setBias(const std::vector<T>& inBias)
{
    for(int i = 0; i < num_filters_out; i++)
    {
        bias(i) = inBias[i];
    }
}

template <typename T, int num_filters_in_t, int num_filters_out_t, int num_features_in_t, int kernel_size_time_t,
    int kernel_size_feature_t, int dilation_rate_t, int stride_t>
void Conv2DT<T, num_filters_in_t, num_filters_out_t, num_features_in_t, kernel_size_time_t, kernel_size_feature_t,
    dilation_rate_t, stride_t>::setWeights(const std::vector<std::vector<std::vector<std::vector<T>>>>& inWeights)
{
    for(int i = 0; i < kernel_size_time; i++)
    {
        conv1dLayers[i].setWeights(inWeights[i]);
    }
}

template <typename T, int num_filters_in_t, int num_filters_out_t, int num_features_in_t, int kernel_size_time_t,
    int kernel_size_feature_t, int dilation_rate_t, int stride_t>
void Conv2DT<T, num_filters_in_t, num_filters_out_t, num_features_in_t, kernel_size_time_t,
    kernel_size_feature_t, dilation_rate_t, stride_t>::setBias(const std::vector<T>& inBias)
{
    for(int i = 0; i < num_filters_out; i++)
    {
        bias(i) = inBias[i];
    }
}
} // RTNeural