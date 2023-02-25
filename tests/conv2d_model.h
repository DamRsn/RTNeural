#pragma once

#include "load_csv.hpp"
#include <RTNeural.h>

/**
 * Computes the receptive field of the model (always 1 if there's no Conv2D layer in the model)
 * Also corresponds to the number of run after a reset before getting a valid/meaningful output
 */
int computeReceptiveField(const RTNeural::Model<TestType>& model)
{
    int receptive_field = 1;

    for(auto* l : model.layers)
    {
        if(l->getName() == "conv2d")
        {
            auto conv = dynamic_cast<RTNeural::Conv2D<TestType>*>(l);
            receptive_field += conv->receptive_field - 1;
        }
    }
    return receptive_field;
}

int computeTotalPaddedLeftFramesTensorflow(const RTNeural::Model<TestType>& model)
{
    int total_pad_left = 0;

    for(auto* l : model.layers)
    {
        if(l->getName() == "conv2d")
        {
            // Following tensorflow padding documentation: https://www.tensorflow.org/api_docs/python/tf/nn#notes_on_padding_2
            // And using the fact that in the time dimension, only stride = 1 is supported:

            auto conv = dynamic_cast<RTNeural::Conv2D<TestType>*>(l);

            if(!conv->valid_pad)
                total_pad_left += (conv->receptive_field - 1) / 2;
        }
    }

    return total_pad_left;
}

void processModelNonT(RTNeural::Model<TestType>& model, const std::vector<TestType>& xData, std::vector<TestType>& yData, int num_frames, int num_features_in, int num_features_out)
{
    model.reset();

    TestType input alignas(RTNEURAL_DEFAULT_ALIGNMENT)[num_features_in];

    for(size_t n = 0; n < num_frames; ++n)
    {
        std::copy(xData.data() + n * num_features_in, xData.data() + (n + 1) * num_features_in, input);
        model.forward(input);
        std::copy(model.getOutputs(), model.getOutputs() + num_features_out, yData.data() + n * num_features_out);
    }
}

template <int numFeaturesIn, typename ModelType>
void processModelT(ModelType& model, const std::vector<TestType>& xData, std::vector<TestType>& yData, int num_frames, int num_features_out)
{
    model.reset();

    TestType input alignas(RTNEURAL_DEFAULT_ALIGNMENT)[numFeaturesIn];

    for(size_t n = 0; n < num_frames; n++)
    {
        std::copy(xData.begin() + n * numFeaturesIn, xData.begin() + (n + 1) * numFeaturesIn, input);
        auto input_mat = Eigen::Map<Eigen::Matrix<TestType, numFeaturesIn, 1>, RTNEURAL_DEFAULT_ALIGNMENT>(input);
        model.forward(input);
        std::copy(model.getOutputs(), model.getOutputs() + num_features_out, yData.data() + n * num_features_out);
    }
}

int conv2d_test()
{
    std::cout << "TESTING CONV2D MODEL..." << std::endl;

    const std::string model_file = "models/conv2d.json";
    const std::string data_file = "test_data/conv2d_x_python.csv";
    const std::string data_file_y = "test_data/conv2d_y_python.csv";

    constexpr double threshold = 1.0e-6;

    std::ifstream pythonX(data_file);
    auto xData = load_csv::loadFile<TestType>(pythonX);

    std::ifstream pythonY(data_file_y);
    auto yDataPython = load_csv::loadFile<TestType>(pythonY);

    int num_features_in;
    int num_frames;
    int num_features_out;

    int model_receptive_field;
    int tensorflow_pad_left;

    // non-templated model
    std::vector<TestType> yData;
    {
        std::cout << "Loading non-templated model" << std::endl;
        std::ifstream jsonStream(model_file, std::ifstream::binary);
        auto modelRef = RTNeural::json_parser::parseJson<TestType>(jsonStream, true);

        if(!modelRef)
        {
            std::cout << "INVALID CONV2D MODEL..." << std::endl;
            return 1;
        }

        model_receptive_field = computeReceptiveField(*modelRef);
        tensorflow_pad_left = computeTotalPaddedLeftFramesTensorflow(*modelRef);

        num_features_in = modelRef->getInSize();
        num_frames = static_cast<int>(xData.size()) / num_features_in;
        num_features_out = modelRef->getOutSize();

        yData.resize(num_frames * num_features_out, (TestType)0);
        processModelNonT(*modelRef, xData, yData, num_frames, num_features_in, num_features_out);
    }

    size_t nErrs = 0;
    auto max_error = (TestType)0;

    // Evaluate only on valid range
    size_t start_frame_python = tensorflow_pad_left;
    size_t start_frame_rtneural = model_receptive_field - 1;
    size_t num_valid_frames = num_frames - start_frame_rtneural;

    // Otherwise, enters shift manually so everything aligns.

    // Check for non templated
    for(size_t n_f = 0; n_f < num_valid_frames; ++n_f)
    {
        for(size_t i = 0; i < num_features_out; ++i)
        {
            auto err = std::abs(yDataPython.at(start_frame_python * num_features_out + n_f * num_features_out + i) - yData.at(start_frame_rtneural * num_features_out + n_f * num_features_out + i));
            if(err > threshold)
            {
                max_error = std::max(err, max_error);
                nErrs++;
            }
        }
    }

    if(nErrs > 0)
    {
        std::cout << "FAIL NON TEMPLATED: " << nErrs << " errors over " + std::to_string(num_valid_frames * num_features_out) + " values!" << std::endl;
        std::cout << "Maximum error: " << max_error << std::endl;
        return 1;
    }

    std::cout << "SUCCESS NON TEMPLATED!" << std::endl
              << std::endl;

#if 0 // MODELT_AVAILABLE
    // templated model
    std::vector<TestType> yDataT(num_frames * num_features_out, (TestType)0);
    {
        std::cout << "Loading templated model" << std::endl;
        RTNeural::ModelT<TestType, 50, 10,
            RTNeural::Conv2DT<TestType, 1, 8, 50, 5, 3, 1, 3>,
            RTNeural::ReLuActivationT<TestType, 16 * 8>,
            RTNeural::Conv2DT<TestType, 8, 1, 16, 5, 7, 5, 1>,
            RTNeural::ReLuActivationT<TestType, 10>>
            modelT;
        //        RTNeural::ModelT<TestType, 1, 1, RTNeural::Conv2DT<TestType, 1, 1, 1, 1, 1, 1, 1>> modelT;

        std::ifstream jsonStream(model_file, std::ifstream::binary);
        modelT.parseJson(jsonStream, true);
        processModelT<50>(modelT, xData, yDataT, num_frames, num_features_out);
    }

    for(size_t n = 0; n < yDataPython.size(); ++n)
    {
        auto err = std::abs(yDataPython.at(n) - yDataT.at(n + shift));
        if(err > threshold)
        {
            max_error = std::max(err, max_error);
            nErrs++;
        }
    }

    if(nErrs > 0)
    {
        std::cout << "FAIL TEMPLATED: " << nErrs << " errors!" << std::endl;
        std::cout << "Maximum error: " << max_error << std::endl;
        return 1;
    }

    std::cout << "SUCCESS TEMPLATED!" << std::endl;

#endif

    return 0;
}
