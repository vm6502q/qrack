//////////////////////////////////////////////////////////////////////////////////////
//
// (C) Daniel Strano and the Qrack contributors 2017-2019. All rights reserved.
//
// QUnit maintains explicit separability of qubits as an optimization on a QEngine.
// See https://arxiv.org/abs/1710.05867
// (The makers of Qrack have no affiliation with the authors of that paper.)
//
// Licensed under the GNU Lesser General Public License V3.
// See LICENSE.md in the project root or https://www.gnu.org/licenses/lgpl-3.0.en.html
// for details.

#pragma once

#include <cfloat>
#include <random>

#include "qinterface.hpp"

namespace Qrack {

// "PhaseShard" optimizations are basically just a very specific "gate fusion" type optimization, where multiple gates
// are composed into single product gates before application to the state vector, to reduce the total number of gates
// that need to be applied. Rather than handling this as a "QFusion" layer optimization, which will typically sit
// BETWEEN a base QEngine set of "shards" and a QUnit that owns them, this particular gate fusion optimization can be
// amenable to avoiding representational entanglement in QUnit in the first place, which QFusion would not help with.
// Firstly, another QFusion would have to be in place ABOVE the QUnit layer, (with QEngine "below,") for this to work.
// Secondly, QFusion is designed to handle more general gate fusion, not specifically controlled phase gates, which are
// entirely commuting among each other and possibly a jumping-off point for further general "Fourier basis"
// optimizations which should probably reside in QUnit, analogous to the |+>/|-> basis changes QUnit takes advantage of
// for "H" gates.

/** Caches controlled gate phase between shards, (as a case of "gate fusion" optimization particularly useful to QUnit)
 */
struct PhaseShard {
    real1 angle0;
    real1 angle1;
    bool isInvert;

    PhaseShard()
        : angle0(ZERO_R1)
        , angle1(ZERO_R1)
    {
    }
};

struct QEngineShard;
typedef QEngineShard* QEngineShardPtr;
typedef PhaseShard* PhaseShardPtr;
typedef std::map<QEngineShardPtr, PhaseShard> ShardToPhaseMap;

/** Associates a QInterface object with a set of bits. */
struct QEngineShard {
    QInterfacePtr unit;
    bitLenInt mapped;
    bool isEmulated;
    bool isProbDirty;
    bool isPhaseDirty;
    complex amp0;
    complex amp1;
    bool isPlusMinus;
    // Shards which this shard controls
    ShardToPhaseMap controlsShards;
    // Shards of which this shard is a target
    ShardToPhaseMap targetOfShards;

    QEngineShard()
        : unit(NULL)
        , mapped(0)
        , isEmulated(false)
        , isProbDirty(false)
        , isPhaseDirty(false)
        , amp0(ONE_CMPLX)
        , amp1(ZERO_CMPLX)
        , isPlusMinus(false)
        , controlsShards()
        , targetOfShards()
    {
    }

    QEngineShard(QInterfacePtr u, const bool& set)
        : unit(u)
        , mapped(0)
        , isEmulated(false)
        , isProbDirty(false)
        , isPhaseDirty(false)
        , amp0(ONE_CMPLX)
        , amp1(ZERO_CMPLX)
        , isPlusMinus(false)
        , controlsShards()
        , targetOfShards()
    {
        amp0 = set ? ZERO_CMPLX : ONE_CMPLX;
        amp1 = set ? ONE_CMPLX : ZERO_CMPLX;
    }

    // Dirty state constructor:
    QEngineShard(QInterfacePtr u, const bitLenInt& mapping)
        : unit(u)
        , mapped(mapping)
        , isEmulated(false)
        , isProbDirty(true)
        , isPhaseDirty(true)
        , amp0(ONE_CMPLX)
        , amp1(ZERO_CMPLX)
        , isPlusMinus(false)
        , controlsShards()
        , targetOfShards()
    {
    }

    ~QEngineShard()
    {
        if (unit && (mapped == 0)) {
            unit->Finish();
        }
    }

    /// Remove another qubit as being a cached control of a phase gate buffer, for "this" as target bit.
    void RemovePhaseControl(QEngineShardPtr p)
    {
        ShardToPhaseMap::iterator phaseShard = targetOfShards.find(p);
        if (phaseShard != targetOfShards.end()) {
            phaseShard->first->controlsShards.erase(this);
            targetOfShards.erase(phaseShard);
        }
    }

    /// Remove another qubit as being a cached target of a phase gate buffer, for "this" as control bit.
    void RemovePhaseTarget(QEngineShardPtr p)
    {
        ShardToPhaseMap::iterator phaseShard = controlsShards.find(p);
        if (phaseShard != controlsShards.end()) {
            phaseShard->first->targetOfShards.erase(this);
            controlsShards.erase(phaseShard);
        }
    }

    /// Initialize a phase gate buffer, with "this" as target bit and a another qubit "p" as control
    void MakePhaseControlledBy(QEngineShardPtr p)
    {
        if (p && (targetOfShards.find(p) == targetOfShards.end())) {
            PhaseShard ps;
            targetOfShards[p] = ps;
            p->controlsShards[this] = ps;
        }
    }

    /// Initialize a phase gate buffer, with "this" as control bit and a another qubit "p" as target
    void MakePhaseControlOf(QEngineShardPtr p)
    {
        if (p && (controlsShards.find(p) == controlsShards.end())) {
            PhaseShard ps;
            controlsShards[p] = ps;
            p->targetOfShards[this] = ps;
        }
    }

    /// "Fuse" phase gate buffer angles, (and initialize the buffer, if necessary,) for the buffer with "this" as target
    /// bit and a another qubit as control
    void AddPhaseAngles(QEngineShardPtr control, real1 angle0Diff, real1 angle1Diff)
    {
        MakePhaseControlledBy(control);

        real1 nAngle0 = targetOfShards[control].angle0 + angle0Diff;
        real1 nAngle1 = targetOfShards[control].angle1 + angle1Diff;

        // Buffers with "angle0" = 0 are actually symmetric (unchanged) under exchange of control and target.
        // We can reduce our number of buffer instances by taking advantage of this kind of symmetry:
        ShardToPhaseMap::iterator controlShard = controlsShards.find(control);
        if (!targetOfShards[control].isInvert && (controlShard != controlsShards.end()) &&
            (abs(controlShard->second.angle0) < (4 * M_PI * min_norm))) {
            nAngle1 += controlShard->second.angle1;
            RemovePhaseTarget(control);
        }

        while (nAngle0 < (-2 * M_PI)) {
            nAngle0 += 4 * M_PI;
        }
        while (nAngle0 >= (2 * M_PI)) {
            nAngle0 -= 4 * M_PI;
        }
        while (nAngle1 < (-2 * M_PI)) {
            nAngle1 += 4 * M_PI;
        }
        while (nAngle1 >= (2 * M_PI)) {
            nAngle1 -= 4 * M_PI;
        }

        if (!targetOfShards[control].isInvert && (abs(nAngle0) < (4 * M_PI * min_norm)) &&
            (abs(nAngle1) < (4 * M_PI * min_norm))) {
            // The buffer is equal to the identity operator, and it can be removed.
            RemovePhaseControl(control);
            return;
        } else {
            targetOfShards[control].angle0 = nAngle0;
            control->controlsShards[this].angle0 = nAngle0;
            targetOfShards[control].angle1 = nAngle1;
            control->controlsShards[this].angle1 = nAngle1;
        }
    }

    void AddInversionAngles(QEngineShardPtr control, real1 angle0Diff, real1 angle1Diff)
    {
        MakePhaseControlledBy(control);

        PhaseShard& targetOfShard = targetOfShards[control];
        targetOfShard.isInvert = !targetOfShard.isInvert;
        std::swap(targetOfShard.angle0, targetOfShard.angle1);

        PhaseShard& controlShard = control->controlsShards[this];
        controlShard.isInvert = !controlShard.isInvert;
        std::swap(controlShard.angle0, controlShard.angle1);

        AddPhaseAngles(control, angle0Diff, angle1Diff);
    }

    /// If an "inversion" gate is applied to a qubit with controlled phase buffers, we can transform the buffers to
    /// commute, instead of incurring the cost of applying the buffers.
    void FlipPhaseAnti()
    {
        ShardToPhaseMap::iterator phaseShard;
        for (phaseShard = targetOfShards.begin(); phaseShard != targetOfShards.end(); phaseShard++) {
            std::swap(phaseShard->second.angle0, phaseShard->second.angle1);
            PhaseShard& remotePhase = phaseShard->first->controlsShards[this];
            std::swap(remotePhase.angle0, remotePhase.angle1);
        }
    }

    bool TryHCommute()
    {
        complex polar0, polar1;
        ShardToPhaseMap::iterator phaseShard;

        for (phaseShard = controlsShards.begin(); phaseShard != controlsShards.end(); phaseShard++) {
            polar0 = std::polar(ONE_R1, phaseShard->second.angle0 / 2);
            polar1 = std::polar(ONE_R1, phaseShard->second.angle1 / 2);
            if (norm(polar0 - polar1) < min_norm) {
                if (phaseShard->second.isInvert) {
                    return false;
                }
            } else if (norm(polar0 + polar1) < min_norm) {
                if (!phaseShard->second.isInvert) {
                    return false;
                }
            } else {
                return false;
            }
        }

        bool didFlip;
        for (phaseShard = targetOfShards.begin(); phaseShard != targetOfShards.end(); phaseShard++) {
            polar0 = std::polar(ONE_R1, phaseShard->second.angle0 / 2);
            polar1 = std::polar(ONE_R1, phaseShard->second.angle1 / 2);
            didFlip = false;
            if (norm(polar0 - polar1) < min_norm) {
                if (phaseShard->second.isInvert) {
                    polar0 = (polar0 + polar1) / (2 * ONE_R1);
                    polar1 = -polar0;
                    didFlip = true;
                }
            } else if (norm(polar0 + polar1) < min_norm) {
                if (!phaseShard->second.isInvert) {
                    polar0 = (polar0 + polar1) / (2 * ONE_R1);
                    polar1 = polar0;
                    didFlip = true;
                }
            } else {
                return false;
            }

            if (didFlip) {
                phaseShard->second.isInvert = !phaseShard->second.isInvert;
                phaseShard->second.angle0 = ((2 * ONE_R1) * arg(polar0));
                phaseShard->second.angle1 = ((2 * ONE_R1) * arg(polar1));

                PhaseShard& remotePhase = phaseShard->first->controlsShards[this];
                remotePhase.isInvert = !remotePhase.isInvert;
                remotePhase.angle0 = phaseShard->second.angle0;
                remotePhase.angle1 = phaseShard->second.angle1;
            }
        }

        return true;
    }

    bool operator==(const QEngineShard& rhs) { return (mapped == rhs.mapped) && (unit == rhs.unit); }
    bool operator!=(const QEngineShard& rhs) { return (mapped != rhs.mapped) || (unit != rhs.unit); }
};

class QUnit;
typedef std::shared_ptr<QUnit> QUnitPtr;

class QUnit : public QInterface {
protected:
    QInterfaceEngine engine;
    QInterfaceEngine subengine;
    int devID;
    std::vector<QEngineShard> shards;
    complex phaseFactor;
    bool doNormalize;
    bool useHostRam;
    bool useRDRAND;
    bool isSparse;
    bool freezeBasis;

    virtual void SetQubitCount(bitLenInt qb)
    {
        shards.resize(qb);
        QInterface::SetQubitCount(qb);
    }

    QInterfacePtr MakeEngine(bitLenInt length, bitCapInt perm);

public:
    QUnit(QInterfaceEngine eng, QInterfaceEngine subEng, bitLenInt qBitCount, bitCapInt initState = 0,
        qrack_rand_gen_ptr rgp = nullptr, complex phaseFac = complex(-999.0, -999.0), bool doNorm = false,
        bool randomGlobalPhase = true, bool useHostMem = true, int deviceID = -1, bool useHardwareRNG = true,
        bool useSparseStateVec = false);
    QUnit(QInterfaceEngine eng, bitLenInt qBitCount, bitCapInt initState = 0, qrack_rand_gen_ptr rgp = nullptr,
        complex phaseFac = complex(-999.0, -999.0), bool doNorm = true, bool randomGlobalPhase = true,
        bool useHostMem = true, int deviceId = -1, bool useHardwareRNG = true, bool useSparseStateVec = false);

    virtual void SetQuantumState(const complex* inputState);
    virtual void GetQuantumState(complex* outputState);
    virtual void GetProbs(real1* outputProbs);
    virtual complex GetAmplitude(bitCapInt perm);
    virtual void SetPermutation(bitCapInt perm, complex phaseFac = complex(-999.0, -999.0));
    using QInterface::Compose;
    virtual bitLenInt Compose(QUnitPtr toCopy);
    virtual bitLenInt Compose(QInterfacePtr toCopy) { return Compose(std::dynamic_pointer_cast<QUnit>(toCopy)); }
    virtual bitLenInt Compose(QUnitPtr toCopy, bitLenInt start);
    virtual bitLenInt Compose(QInterfacePtr toCopy, bitLenInt start)
    {
        return Compose(std::dynamic_pointer_cast<QUnit>(toCopy), start);
    }
    virtual void Decompose(bitLenInt start, bitLenInt length, QInterfacePtr dest)
    {
        Decompose(start, length, std::dynamic_pointer_cast<QUnit>(dest));
    }
    virtual void Decompose(bitLenInt start, bitLenInt length, QUnitPtr dest);
    virtual void Dispose(bitLenInt start, bitLenInt length);

    /**
     * \defgroup BasicGates Basic quantum gate primitives
     *@{
     */

    using QInterface::H;
    virtual void H(bitLenInt target);
    using QInterface::X;
    virtual void X(bitLenInt target);
    using QInterface::Z;
    virtual void Z(bitLenInt target);
    using QInterface::CNOT;
    virtual void CNOT(bitLenInt control, bitLenInt target);
    using QInterface::AntiCNOT;
    virtual void AntiCNOT(bitLenInt control, bitLenInt target);
    using QInterface::CCNOT;
    virtual void CCNOT(bitLenInt control1, bitLenInt control2, bitLenInt target);
    using QInterface::AntiCCNOT;
    virtual void AntiCCNOT(bitLenInt control1, bitLenInt control2, bitLenInt target);
    using QInterface::CZ;
    virtual void CZ(bitLenInt control, bitLenInt target);

    virtual void ApplySinglePhase(
        const complex topLeft, const complex bottomRight, bool doCalcNorm, bitLenInt qubitIndex);
    virtual void ApplySingleInvert(
        const complex topRight, const complex bottomLeft, bool doCalcNorm, bitLenInt qubitIndex);
    virtual void ApplyControlledSinglePhase(const bitLenInt* controls, const bitLenInt& controlLen,
        const bitLenInt& target, const complex topLeft, const complex bottomRight);
    virtual void ApplyControlledSingleInvert(const bitLenInt* controls, const bitLenInt& controlLen,
        const bitLenInt& target, const complex topRight, const complex bottomLeft);
    virtual void ApplyAntiControlledSinglePhase(const bitLenInt* controls, const bitLenInt& controlLen,
        const bitLenInt& target, const complex topLeft, const complex bottomRight);
    virtual void ApplyAntiControlledSingleInvert(const bitLenInt* controls, const bitLenInt& controlLen,
        const bitLenInt& target, const complex topRight, const complex bottomLeft);
    virtual void ApplySingleBit(const complex* mtrx, bool doCalcNorm, bitLenInt qubit);
    virtual void ApplyControlledSingleBit(
        const bitLenInt* controls, const bitLenInt& controlLen, const bitLenInt& target, const complex* mtrx);
    virtual void ApplyAntiControlledSingleBit(
        const bitLenInt* controls, const bitLenInt& controlLen, const bitLenInt& target, const complex* mtrx);
    using QInterface::UniformlyControlledSingleBit;
    virtual void CSwap(
        const bitLenInt* controls, const bitLenInt& controlLen, const bitLenInt& qubit1, const bitLenInt& qubit2);
    virtual void AntiCSwap(
        const bitLenInt* controls, const bitLenInt& controlLen, const bitLenInt& qubit1, const bitLenInt& qubit2);
    virtual void CSqrtSwap(
        const bitLenInt* controls, const bitLenInt& controlLen, const bitLenInt& qubit1, const bitLenInt& qubit2);
    virtual void AntiCSqrtSwap(
        const bitLenInt* controls, const bitLenInt& controlLen, const bitLenInt& qubit1, const bitLenInt& qubit2);
    virtual void CISqrtSwap(
        const bitLenInt* controls, const bitLenInt& controlLen, const bitLenInt& qubit1, const bitLenInt& qubit2);
    virtual void AntiCISqrtSwap(
        const bitLenInt* controls, const bitLenInt& controlLen, const bitLenInt& qubit1, const bitLenInt& qubit2);
    using QInterface::ForceM;
    virtual bool ForceM(bitLenInt qubitIndex, bool result, bool doForce = true);

    /** @} */

    /**
     * \defgroup LogicGates Logic Gates
     *
     * Each bit is paired with a CL* variant that utilizes a classical bit as
     * an input.
     *
     * @{
     */

    using QInterface::AND;
    virtual void AND(bitLenInt inputBit1, bitLenInt inputBit2, bitLenInt outputBit, bitLenInt length);
    using QInterface::OR;
    virtual void OR(bitLenInt inputBit1, bitLenInt inputBit2, bitLenInt outputBit, bitLenInt length);
    using QInterface::XOR;
    virtual void XOR(bitLenInt inputBit1, bitLenInt inputBit2, bitLenInt outputBit, bitLenInt length);
    using QInterface::CLAND;
    virtual void CLAND(bitLenInt qInputStart, bitCapInt classicalInput, bitLenInt outputStart, bitLenInt length);
    using QInterface::CLOR;
    virtual void CLOR(bitLenInt qInputStart, bitCapInt classicalInput, bitLenInt outputStart, bitLenInt length);
    using QInterface::CLXOR;
    virtual void CLXOR(bitLenInt qInputStart, bitCapInt classicalInput, bitLenInt outputStart, bitLenInt length);

    /** @} */

    /**
     * \defgroup ArithGate Arithmetic and other opcode-like gate implemenations.
     *
     * @{
     */

    virtual void INC(bitCapInt toAdd, bitLenInt start, bitLenInt length);
    virtual void CINC(
        bitCapInt toAdd, bitLenInt inOutStart, bitLenInt length, bitLenInt* controls, bitLenInt controlLen);
    virtual void INCC(bitCapInt toAdd, bitLenInt start, bitLenInt length, bitLenInt carryIndex);
    virtual void INCS(bitCapInt toAdd, bitLenInt start, bitLenInt length, bitLenInt overflowIndex);
    virtual void INCSC(
        bitCapInt toAdd, bitLenInt start, bitLenInt length, bitLenInt overflowIndex, bitLenInt carryIndex);
    virtual void INCSC(bitCapInt toAdd, bitLenInt start, bitLenInt length, bitLenInt carryIndex);
    virtual void INCBCD(bitCapInt toAdd, bitLenInt start, bitLenInt length);
    virtual void INCBCDC(bitCapInt toAdd, bitLenInt start, bitLenInt length, bitLenInt carryIndex);
    virtual void DECC(bitCapInt toSub, bitLenInt start, bitLenInt length, bitLenInt carryIndex);
    virtual void DECSC(
        bitCapInt toAdd, bitLenInt start, bitLenInt length, bitLenInt overflowIndex, bitLenInt carryIndex);
    virtual void DECSC(bitCapInt toAdd, bitLenInt start, bitLenInt length, bitLenInt carryIndex);
    virtual void DECBCD(bitCapInt toAdd, bitLenInt start, bitLenInt length);
    virtual void DECBCDC(bitCapInt toSub, bitLenInt start, bitLenInt length, bitLenInt carryIndex);
    virtual void MUL(bitCapInt toMul, bitLenInt inOutStart, bitLenInt carryStart, bitLenInt length);
    virtual void DIV(bitCapInt toDiv, bitLenInt inOutStart, bitLenInt carryStart, bitLenInt length);
    virtual void MULModNOut(bitCapInt toMul, bitCapInt modN, bitLenInt inStart, bitLenInt outStart, bitLenInt length);
    virtual void IMULModNOut(bitCapInt toMul, bitCapInt modN, bitLenInt inStart, bitLenInt outStart, bitLenInt length);
    virtual void POWModNOut(bitCapInt base, bitCapInt modN, bitLenInt inStart, bitLenInt outStart, bitLenInt length);
    virtual void CMUL(bitCapInt toMul, bitLenInt inOutStart, bitLenInt carryStart, bitLenInt length,
        bitLenInt* controls, bitLenInt controlLen);
    virtual void CDIV(bitCapInt toDiv, bitLenInt inOutStart, bitLenInt carryStart, bitLenInt length,
        bitLenInt* controls, bitLenInt controlLen);
    virtual void CMULModNOut(bitCapInt toMul, bitCapInt modN, bitLenInt inStart, bitLenInt outStart, bitLenInt length,
        bitLenInt* controls, bitLenInt controlLen);
    virtual void CIMULModNOut(bitCapInt toMul, bitCapInt modN, bitLenInt inStart, bitLenInt outStart, bitLenInt length,
        bitLenInt* controls, bitLenInt controlLen);
    virtual void CPOWModNOut(bitCapInt base, bitCapInt modN, bitLenInt inStart, bitLenInt outStart, bitLenInt length,
        bitLenInt* controls, bitLenInt controlLen);

    /** @} */

    /**
     * \defgroup ExtraOps Extra operations and capabilities
     *
     * @{
     */

    virtual void ZeroPhaseFlip(bitLenInt start, bitLenInt length);
    virtual void CPhaseFlipIfLess(bitCapInt greaterPerm, bitLenInt start, bitLenInt length, bitLenInt flagIndex);
    virtual void PhaseFlipIfLess(bitCapInt greaterPerm, bitLenInt start, bitLenInt length);
    virtual void PhaseFlip();
    virtual void SetReg(bitLenInt start, bitLenInt length, bitCapInt value);
    virtual bitCapInt IndexedLDA(bitLenInt indexStart, bitLenInt indexLength, bitLenInt valueStart,
        bitLenInt valueLength, unsigned char* values);
    virtual bitCapInt IndexedADC(bitLenInt indexStart, bitLenInt indexLength, bitLenInt valueStart,
        bitLenInt valueLength, bitLenInt carryIndex, unsigned char* values);
    virtual bitCapInt IndexedSBC(bitLenInt indexStart, bitLenInt indexLength, bitLenInt valueStart,
        bitLenInt valueLength, bitLenInt carryIndex, unsigned char* values);
    virtual void Swap(bitLenInt qubit1, bitLenInt qubit2);
    virtual void ISwap(bitLenInt qubit1, bitLenInt qubit2);
    virtual void SqrtSwap(bitLenInt qubit1, bitLenInt qubit2);
    virtual void ISqrtSwap(bitLenInt qubit1, bitLenInt qubit2);

    /** @} */

    /**
     * \defgroup UtilityFunc Utility functions
     *
     * @{
     */

    virtual real1 Prob(bitLenInt qubit);
    virtual real1 ProbAll(bitCapInt fullRegister);
    virtual bool ApproxCompare(QInterfacePtr toCompare)
    {
        return ApproxCompare(std::dynamic_pointer_cast<QUnit>(toCompare));
    }
    virtual bool ApproxCompare(QUnitPtr toCompare);
    virtual void UpdateRunningNorm();
    virtual void NormalizeState(real1 nrm = -999.0);
    virtual void Finish();
    virtual bool isFinished();

    virtual bool TrySeparate(bitLenInt start, bitLenInt length = 1);

    virtual QInterfacePtr Clone();

    /** @} */

protected:
    virtual void XBase(const bitLenInt& target);
    virtual void ZBase(const bitLenInt& target);
    virtual real1 ProbBase(const bitLenInt& qubit);

    virtual void UniformlyControlledSingleBit(const bitLenInt* controls, const bitLenInt& controlLen,
        bitLenInt qubitIndex, const complex* mtrxs, const bitCapInt* mtrxSkipPowers, const bitLenInt mtrxSkipLen,
        const bitCapInt& mtrxSkipValueMask);

    typedef void (QInterface::*INCxFn)(bitCapInt, bitLenInt, bitLenInt, bitLenInt);
    typedef void (QInterface::*INCxxFn)(bitCapInt, bitLenInt, bitLenInt, bitLenInt, bitLenInt);
    typedef void (QInterface::*CMULFn)(bitCapInt toMod, bitLenInt start, bitLenInt carryStart, bitLenInt length,
        bitLenInt* controls, bitLenInt controlLen);
    typedef void (QInterface::*CMULModFn)(bitCapInt toMod, bitCapInt modN, bitLenInt start, bitLenInt carryStart,
        bitLenInt length, bitLenInt* controls, bitLenInt controlLen);
    void CollapseCarry(bitLenInt flagIndex, bitLenInt start, bitLenInt length);
    void INT(bitCapInt toMod, bitLenInt start, bitLenInt length, bitLenInt carryIndex, bool hasCarry,
        bitLenInt* controls = NULL, bitLenInt controlLen = 0);
    void INTS(bitCapInt toMod, bitLenInt start, bitLenInt length, bitLenInt overflowIndex, bitLenInt carryIndex,
        bool hasCarry);
    void INCx(INCxFn fn, bitCapInt toMod, bitLenInt start, bitLenInt length, bitLenInt flagIndex);
    void INCxx(
        INCxxFn fn, bitCapInt toMod, bitLenInt start, bitLenInt length, bitLenInt flag1Index, bitLenInt flag2Index);
    QInterfacePtr CMULEntangle(std::vector<bitLenInt> controlVec, bitLenInt start, bitCapInt carryStart,
        bitLenInt length, std::vector<bitLenInt>* controlsMapped);
    std::vector<bitLenInt> CMULEntangle(
        std::vector<bitLenInt> controlVec, bitLenInt start, bitCapInt carryStart, bitLenInt length);
    void CMULx(CMULFn fn, bitCapInt toMod, bitLenInt start, bitLenInt carryStart, bitLenInt length, bitLenInt* controls,
        bitLenInt controlLen);
    void xMULModNOut(
        bitCapInt toMul, bitCapInt modN, bitLenInt inStart, bitLenInt outStart, bitLenInt length, bool inverse);
    void CxMULModNOut(bitCapInt toMul, bitCapInt modN, bitLenInt inStart, bitLenInt outStart, bitLenInt length,
        bitLenInt* controls, bitLenInt controlLen, bool inverse);
    void CMULModx(CMULModFn fn, bitCapInt toMod, bitCapInt modN, bitLenInt start, bitLenInt carryStart,
        bitLenInt length, std::vector<bitLenInt> controlVec);
    bool CArithmeticOptimize(bitLenInt* controls, bitLenInt controlLen, std::vector<bitLenInt>* controlVec);
    bool INTCOptimize(bitCapInt toMod, bitLenInt start, bitLenInt length, bool isAdd, bitLenInt carryIndex);
    bool INTSOptimize(bitCapInt toMod, bitLenInt start, bitLenInt length, bool isAdd, bitLenInt overflowIndex);
    bool INTSCOptimize(
        bitCapInt toMod, bitLenInt start, bitLenInt length, bool isAdd, bitLenInt carryIndex, bitLenInt overflowIndex);

    template <typename F>
    void CBoolReg(const bitLenInt& qInputStart, const bitCapInt& classicalInput, const bitLenInt& outputStart,
        const bitLenInt& length, F fn);

    virtual QInterfacePtr Entangle(std::vector<bitLenInt*> bits);
    virtual QInterfacePtr EntangleRange(bitLenInt start, bitLenInt length);
    virtual QInterfacePtr EntangleRange(bitLenInt start, bitLenInt length, bitLenInt start2, bitLenInt length2);
    virtual QInterfacePtr EntangleRange(
        bitLenInt start, bitLenInt length, bitLenInt start2, bitLenInt length2, bitLenInt start3, bitLenInt length3);
    virtual QInterfacePtr EntangleAll();

    virtual bool CheckBitPermutation(const bitLenInt& qubitIndex, const bool& inCurrentBasis = false);
    virtual bool CheckBitsPermutation(
        const bitLenInt& start, const bitLenInt& length, const bool& inCurrentBasis = false);
    virtual bitCapInt GetCachedPermutation(const bitLenInt& start, const bitLenInt& length);
    virtual bitCapInt GetCachedPermutation(const bitLenInt* bitArray, const bitLenInt& length);

    virtual QInterfacePtr EntangleInCurrentBasis(
        std::vector<bitLenInt*>::iterator first, std::vector<bitLenInt*>::iterator last);

    template <typename F, typename... B> void EntangleAndCallMember(F fn, B... bits);
    template <typename F, typename... B> void EntangleAndCall(F fn, B... bits);
    template <typename F, typename... B> void EntangleAndCallMemberRot(F fn, real1 radians, B... bits);

    typedef bool (*ParallelUnitFn)(QInterfacePtr unit, real1 param);
    bool ParallelUnitApply(ParallelUnitFn fn, real1 param = ZERO_R1);

    virtual void SeparateBit(bool value, bitLenInt qubit);

    void OrderContiguous(QInterfacePtr unit);

    virtual void Detach(bitLenInt start, bitLenInt length, QUnitPtr dest);

    struct QSortEntry {
        bitLenInt bit;
        bitLenInt mapped;
        bool operator<(const QSortEntry& rhs) { return mapped < rhs.mapped; }
        bool operator>(const QSortEntry& rhs) { return mapped > rhs.mapped; }
    };
    void SortUnit(QInterfacePtr unit, std::vector<QSortEntry>& bits, bitLenInt low, bitLenInt high);

    template <typename CF, typename F>
    void ApplyEitherControlled(const bitLenInt* controls, const bitLenInt& controlLen,
        const std::vector<bitLenInt> targets, const bool& anti, CF cfn, F f, const bool& inCurrentBasis = false);

    bitCapInt GetIndexedEigenstate(bitLenInt indexStart, bitLenInt indexLength, bitLenInt valueStart,
        bitLenInt valueLength, unsigned char* values);

    void Transform2x2(const complex* mtrxIn, complex* mtrxOut);
    void TransformPhase(const complex& topLeft, const complex& bottomRight, complex* mtrxOut);
    void TransformInvert(const complex& topRight, const complex& bottomLeft, complex* mtrxOut);

    void TransformBasis1Qb(const bool& toPlusMinus, const bitLenInt& i);

    void RevertBasis2Qb(const bitLenInt& i);
    void RevertBasis2Qb(const bitLenInt& start, const bitLenInt& length)
    {
        for (bitLenInt i = 0; i < length; i++) {
            RevertBasis2Qb(start + i);
        }
    }
    void ToPermBasis(const bitLenInt& i)
    {
        TransformBasis1Qb(false, i);
        RevertBasis2Qb(i);
    }
    void ToPermBasis(const bitLenInt& start, const bitLenInt& length)
    {
        for (bitLenInt i = 0; i < length; i++) {
            ToPermBasis(start + i);
        }
    }
    void ToPermBasisAll() { ToPermBasis(0, qubitCount); }
    void PopHBasis2Qb(const bitLenInt& i)
    {
        QEngineShard& shard = shards[i];
        if (shard.isPlusMinus && ((shard.targetOfShards.size() != 0) || (shard.controlsShards.size() != 0))) {
            TransformBasis1Qb(false, i);
        }
    }

    void CheckShardSeparable(const bitLenInt& target);

    void DirtyShardRange(bitLenInt start, bitLenInt length)
    {
        for (bitLenInt i = 0; i < length; i++) {
            shards[start + i].isProbDirty = true;
            shards[start + i].isPhaseDirty = true;
        }
    }

    void DirtyShardRangePhase(bitLenInt start, bitLenInt length)
    {
        for (bitLenInt i = 0; i < length; i++) {
            shards[start + i].isPhaseDirty = true;
        }
    }

    void DirtyShardIndexArray(bitLenInt* bitIndices, bitLenInt length)
    {
        for (bitLenInt i = 0; i < length; i++) {
            shards[bitIndices[i]].isProbDirty = true;
            shards[bitIndices[i]].isPhaseDirty = true;
        }
    }

    void DirtyShardIndexVector(std::vector<bitLenInt> bitIndices)
    {
        for (bitLenInt i = 0; i < bitIndices.size(); i++) {
            shards[bitIndices[i]].isProbDirty = true;
            shards[bitIndices[i]].isPhaseDirty = true;
        }
    }

    void EndEmulation(QEngineShard& shard)
    {
        if (shard.isEmulated) {
            complex bitState[2] = { shard.amp0, shard.amp1 };
            shard.unit->SetQuantumState(bitState);
            shard.isEmulated = false;
        }
    }

    void EndEmulation(const bitLenInt& target)
    {
        QEngineShard& shard = shards[target];
        EndEmulation(shard);
    }

    void EndEmulation(bitLenInt* bitIndices, bitLenInt length)
    {
        for (bitLenInt i = 0; i < length; i++) {
            EndEmulation(bitIndices[i]);
        }
    }

    void EndAllEmulation()
    {
        for (bitLenInt i = 0; i < qubitCount; i++) {
            EndEmulation(i);
        }
    }

    template <typename F> void ApplyOrEmulate(QEngineShard& shard, F payload)
    {
        if ((shard.unit->GetQubitCount() == 1) && !shard.isProbDirty && !shard.isPhaseDirty) {
            shard.isEmulated = true;
        } else {
            payload(shard);
        }
    }

    bitLenInt FindShardIndex(const QEngineShard& shard)
    {
        for (bitLenInt i = 0; i < shards.size(); i++) {
            if (shards[i] == shard) {
                return i;
            }
        }
        return shards.size();
    }

    bool TryCnotOptimize(const bitLenInt* controls, const bitLenInt& controlLen, const bitLenInt& target,
        const complex& bottomLeft, const complex& topRight, const bool& anti);

    /* Debugging and diagnostic routines. */
    void DumpShards();
    QInterfacePtr GetUnit(bitLenInt bit) { return shards[bit].unit; }
};

} // namespace Qrack
