// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/chrome_report_unrecoverable_error.h"

#include "chrome/browser/sync/glue/data_type_controller.h"
#include "sync/util/data_type_histogram.h"

namespace browser_sync {

bool DataTypeController::IsUnrecoverableResult(StartResult result) {
  return (result == ASSOCIATION_FAILED || result == UNRECOVERABLE_ERROR);
}

void DataTypeController::RecordUnrecoverableError(
    const tracked_objects::Location& from_here,
    const std::string& message) {
  DVLOG(1) << "Datatype Controller failed for type "
           << ModelTypeToString(type()) << "  "
           << message << " at location "
           << from_here.ToString();
  UMA_HISTOGRAM_ENUMERATION("Sync.DataTypeRunFailures", type(),
                            syncable::MODEL_TYPE_COUNT);
  ChromeReportUnrecoverableError();
}

}
