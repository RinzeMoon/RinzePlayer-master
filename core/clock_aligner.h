// SPDX-FileCopyrightText: 2025-2026 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef CLOCK_ALIGNER_H
#define CLOCK_ALIGNER_H

#include <QObject>
#include <QDateTime>

class ClockAligner {
public:
    ClockAligner() = default;

    // Host: record t1 before sending handshake
    qint64 start();

    // Guest: record t1 from the host's handshake
    void recordT1(qint64 t1) { m_t1 = t1; }

    // Guest: return t2
    qint64 respond();

    // Host: complete the handshake with t2 from guest
    void complete(qint64 t2);

    // Apply offset to a timestamp from the peer
    qint64 toLocal(qint64 peerTs) const { return peerTs + m_offsetMs; }

    // Apply offset to local ts for the peer
    qint64 toPeer(qint64 localTs) const { return localTs - m_offsetMs; }

    double offsetMs() const { return (double)m_offsetMs; }
    bool aligned() const { return m_aligned; }

private:
    qint64 m_t1 = 0;
    qint64 m_t2 = 0;
    qint64 m_t3 = 0;
    qint64 m_offsetMs = 0;
    bool m_aligned = false;
};

#endif // CLOCK_ALIGNER_H
