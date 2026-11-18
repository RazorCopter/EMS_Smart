// SPDX-License-Identifier: MIT
#include "dashboard_gzip.h"
#include <cstddef>
namespace heliograph::v51 {
const unsigned char* dashboardPage() {return kDashboard;}
size_t dashboardSize() {return sizeof(kDashboard);}
}
