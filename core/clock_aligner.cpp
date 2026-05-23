// SPDX-FileCopyrightText: 2025-2026 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later

#include "clock_aligner.h"

qint64 ClockAligner::start()
{
    m_t1 = QDateTime::currentMSecsSinceEpoch();
    return m_t1;
}

qint64 ClockAligner::respond()
{
    m_t2 = QDateTime::currentMSecsSinceEpoch();
    return m_t2;
}

void ClockAligner::complete(qint64 t2)
{
    m_t3 = QDateTime::currentMSecsSinceEpoch();
    m_t2 = t2;
    // offset = (t2 - t1 - (t3 - t2)) / 2
    m_offsetMs = (m_t2 - m_t1 - (m_t3 - m_t2)) / 2;
    m_aligned = true;
}
