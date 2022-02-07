// Copyright (C) 2018-2022 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#include "openvino/runtime/compiled_model.hpp"

#include <pybind11/stl.h>

#include <pyopenvino/graph/any.hpp>

#include "common.hpp"
#include "pyopenvino/core/containers.hpp"
#include "pyopenvino/core/infer_request.hpp"

PYBIND11_MAKE_OPAQUE(Containers::TensorIndexMap);
PYBIND11_MAKE_OPAQUE(Containers::TensorNameMap);

namespace py = pybind11;

void regclass_CompiledModel(py::module m) {
    py::class_<ov::CompiledModel, std::shared_ptr<ov::CompiledModel>> cls(m, "CompiledModel");

    cls.def(py::init([](ov::CompiledModel& other) {
                return other;
            }),
            py::arg("other"));

    cls.def("create_infer_request", [](ov::CompiledModel& self) {
        return std::make_shared<InferRequestWrapper>(self.create_infer_request(), self.inputs(), self.outputs());
    });

    cls.def(
        "infer_new_request",
        [](ov::CompiledModel& self, const py::dict& inputs) {
            auto request = self.create_infer_request();
            // Update inputs if there are any
            Common::set_request_tensors(request, inputs);
            request.infer();
            return Common::outputs_to_dict(self.outputs(), request);
        },
        py::arg("inputs"));

    cls.def("export_model", &ov::CompiledModel::export_model, py::arg("model_stream"));

    cls.def(
        "set_property",
        [](ov::CompiledModel& self, const std::map<std::string, py::object>& properties) {
            std::map<std::string, PyAny> properties_to_cpp;
            for (const auto& property : properties) {
                properties_to_cpp[property.first] = PyAny(property.second);
            }
            self.set_property({properties_to_cpp.begin(), properties_to_cpp.end()});
        },
        py::arg("properties"),
        R"(
            Sets properties for current compiled model.
            Parameters
            ----------
            properties : dict
                Dict of pairs: (property name, property value)
            Returns
            ----------
            set_property : None
        )");

    // todo: remove after Accuracy Checker migration to set/get_property API
    cls.def(
        "get_config",
        [](ov::CompiledModel& self, const std::string& name) -> py::object {
            PyErr_WarnEx(PyExc_DeprecationWarning, "get_config() is deprecated, use get_property() instead.", 1);
            return Common::from_ov_any(self.get_property(name)).as<py::object>();
        },
        py::arg("name"));

    cls.def(
        "get_property",
        [](ov::CompiledModel& self, const std::string& name) -> py::object {
            return Common::from_ov_any(self.get_property(name)).as<py::object>();
        },
        py::arg("name"),
        R"(
            Gets properties for current compiled model.
            Parameters
            ----------
            name : str
                Property name.
            Returns
            ----------
            get_property : Any
        )");

    // todo: remove after Accuracy Checker migration to set/get_property API
    cls.def(
        "get_metric",
        [](ov::CompiledModel& self, const std::string& name) -> py::object {
            PyErr_WarnEx(PyExc_DeprecationWarning, "get_metric() is deprecated, use get_property() instead.", 1);
            return Common::from_ov_any(self.get_property(name)).as<py::object>();
        },
        py::arg("name"));

    cls.def("get_runtime_model", &ov::CompiledModel::get_runtime_model);

    cls.def_property_readonly("inputs", &ov::CompiledModel::inputs);

    cls.def("input", (ov::Output<const ov::Node>(ov::CompiledModel::*)() const) & ov::CompiledModel::input);

    cls.def("input",
            (ov::Output<const ov::Node>(ov::CompiledModel::*)(size_t) const) & ov::CompiledModel::input,
            py::arg("index"));

    cls.def("input",
            (ov::Output<const ov::Node>(ov::CompiledModel::*)(const std::string&) const) & ov::CompiledModel::input,
            py::arg("tensor_name"));

    cls.def_property_readonly("outputs", &ov::CompiledModel::outputs);

    cls.def("output", (ov::Output<const ov::Node>(ov::CompiledModel::*)() const) & ov::CompiledModel::output);

    cls.def("output",
            (ov::Output<const ov::Node>(ov::CompiledModel::*)(size_t) const) & ov::CompiledModel::output,
            py::arg("index"));

    cls.def("output",
            (ov::Output<const ov::Node>(ov::CompiledModel::*)(const std::string&) const) & ov::CompiledModel::output,
            py::arg("tensor_name"));
}
