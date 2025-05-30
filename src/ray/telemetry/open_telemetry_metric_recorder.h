// Copyright 2025 The Ray Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//  http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

#include <opentelemetry/metrics/meter.h>
#include <opentelemetry/metrics/observer_result.h>
#include <opentelemetry/metrics/sync_instruments.h>
#include <opentelemetry/sdk/metrics/meter_provider.h>

#include <cassert>
#include <chrono>
#include <map>
#include <mutex>
#include <optional>
#include <unordered_map>
#include <vector>

#include "absl/container/flat_hash_map.h"

namespace ray {
namespace telemetry {

// OpenTelemetryMetricRecorder is a singleton class that initializes the OpenTelemetry
// grpc exporter and creates a Meter for recording metrics. It is responsible for
// exporting metrics to a repoter_agent.py endpoint at a given interval.
class OpenTelemetryMetricRecorder {
 public:
  // Returns the singleton instance of OpenTelemetryMetricRecorder. This should be
  // called after Register() to ensure the instance is initialized.
  static OpenTelemetryMetricRecorder &GetInstance();

  // Registers the OpenTelemetryMetricRecorder with the specified grpc endpoint,
  // interval and timeout. This should be called only once per process.
  void RegisterGrpcExporter(const std::string &endpoint,
                            std::chrono::milliseconds interval,
                            std::chrono::milliseconds timeout);

  // Flush the remaining metrics and reset the OpenTelemetryMetricRecorder. Note that
  // this is a reset rather than a complete shutdown, so it can be consistent with
  // the stats.h behavior.
  void Shutdown();

  // Registers a gauge metric with the given name and description
  void RegisterGaugeMetric(const std::string &name, const std::string &description);

  // Registers a counter metric with the given name and description
  void RegisterCounterMetric(const std::string &name, const std::string &description);

  // Set the value of a metric given the tags and the metric value.
  bool SetMetricValue(const std::string &name,
                      absl::flat_hash_map<std::string, std::string> &&tags,
                      double value);

  // Get the value of a metric given the tags.
  std::optional<double> GetObservableMetricValue(
      const std::string &name,
      const absl::flat_hash_map<std::string, std::string> &tags) const;

  // Helper function to collect gauge metric values. This function is called only once
  // per interval for each metric. It collects the values from the observations_by_name_
  // map and passes them to the observer.
  void CollectGaugeMetricValues(
      const std::string &name,
      const std::shared_ptr<opentelemetry::metrics::ObserverResultT<double>> &observer);

  // Delete copy constructors and assignment operators. Skip generation of the move
  // constructors and assignment operators.
  OpenTelemetryMetricRecorder(const OpenTelemetryMetricRecorder &) = delete;
  OpenTelemetryMetricRecorder &operator=(const OpenTelemetryMetricRecorder &) = delete;
  ~OpenTelemetryMetricRecorder() = default;

 private:
  OpenTelemetryMetricRecorder();
  std::shared_ptr<opentelemetry::sdk::metrics::MeterProvider> meter_provider_;

  // Map of metric names to their observations (aka. set of tags and metric values).
  // This contains all data points for a given metric for a given interval. This map is
  // cleared at the end of each interval.
  absl::flat_hash_map<
      std::string,
      absl::flat_hash_map<absl::flat_hash_map<std::string, std::string>, double>>
      observations_by_name_;
  // Map of metric names to their observable instrument pointers. This is used to ensure
  // that each metric is only registered once.
  absl::flat_hash_map<std::string,
                      std::shared_ptr<opentelemetry::metrics::ObservableInstrument>>
      registered_observable_instruments_;
  // Map of metric names to their synchronous instrument pointers. This is used to ensure
  // that each metric is only registered once.
  absl::flat_hash_map<std::string,
                      std::unique_ptr<opentelemetry::metrics::SynchronousInstrument>>
      registered_synchronous_instruments_;
  // Lock for thread safety when modifying state.
  std::mutex mutex_;

  bool setObservableMetricValue(const std::string &name,
                                absl::flat_hash_map<std::string, std::string> &&tags,
                                double value);

  bool setSynchronousMetricValue(const std::string &name,
                                 absl::flat_hash_map<std::string, std::string> &&tags,
                                 double value);

  bool isObservableMetric(const std::string &name) const {
    return registered_observable_instruments_.find(name) !=
           registered_observable_instruments_.end();
  }

  std::shared_ptr<opentelemetry::metrics::Meter> getMeter() {
    return meter_provider_->GetMeter("ray");
  }
};
}  // namespace telemetry
}  // namespace ray
