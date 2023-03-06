/*
// Copyright (C) 2021-2023 Intel Corporation
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
*/

#include "models/model_base.h"
#include <models/results.h>
#include "utils/args_helper.hpp"
#include <adapters/openvino_adapter.h>

#include <utility>

#include <openvino/openvino.hpp>

#include <utils/common.hpp>
#include <utils/config_factory.h>
#include <utils/ocv_common.hpp>
#include <utils/slog.hpp>

void ModelBase::load(std::shared_ptr<InferenceAdapter> adapter) {
    if (!adapter) {
        inferenceAdapter = std::make_shared<OpenVINOInferenceAdapter>();
    } else {
        inferenceAdapter = adapter;
    }

    auto core = ov::Core();
    prepareModel(core);
}

void ModelBase::prepareModel(ov::Core& core) {
    // --------------------------- Read IR Generated by ModelOptimizer (.xml and .bin files) ------------
    /** Read model **/
    slog::info << "Reading model " << modelFile << slog::endl;
    std::shared_ptr<ov::Model> model = core.read_model(modelFile);
    logBasicModelInfo(model);
    // -------------------------- Reading all outputs names and customizing I/O tensors (in inherited classes)
    prepareInputsOutputs(model);

    /** Set batch size to 1 **/
    ov::set_batch(model, 1);    

    inferenceAdapter->loadModel(model, core, "AUTO", config.compilationConfig);
}

ov::Layout ModelBase::getInputLayout(const ov::Output<ov::Node>& input) {
    const ov::Shape& inputShape = input.get_shape();
    ov::Layout layout = ov::layout::get_layout(input);
    if (layout.empty()) {
        if (inputsLayouts.empty()) {
            layout = getLayoutFromShape(inputShape);
            slog::warn << "Automatically detected layout '" << layout.to_string() << "' for input '"
                       << input.get_any_name() << "' will be used." << slog::endl;
        } else if (inputsLayouts.size() == 1) {
            layout = inputsLayouts.begin()->second;
        } else {
            layout = inputsLayouts[input.get_any_name()];
        }
    }

    return layout;
}

std::unique_ptr<ResultBase> ModelBase::infer(const InputData& inputData)
{
    InferenceInput inputs;
    InferenceResult result;
    auto internalModelData = this->preprocess(inputData, inputs);
    
    result.outputsData = inferenceAdapter->infer(inputs);
    result.internalModelData = std::move(internalModelData);

    auto retVal = this->postprocess(result);
    *retVal = static_cast<ResultBase&>(result);
    return retVal;
}