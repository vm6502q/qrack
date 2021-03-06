//////////////////////////////////////////////////////////////////////////////////////
//
// (C) Daniel Strano and the Qrack contributors 2017-2021. All rights reserved.
//
// This is a multithreaded, universal quantum register simulation, allowing
// (nonphysical) register cloning and direct measurement of probability and
// phase, to leverage what advantages classical emulation of qubits can have.
//
// Licensed under the GNU Lesser General Public License V3.
// See LICENSE.md in the project root or https://www.gnu.org/licenses/lgpl-3.0.en.html
// for details.

#include "qinterface.hpp"

#include <future>

namespace Qrack {

/// "Phase shift gate" - Rotates as e^(-i*\theta/2) around |1> state
void QInterface::RT(real1_f radians, bitLenInt qubit)
{
    ApplySinglePhase(complex(ONE_R1, ZERO_R1), complex((real1)cos(radians / 2), (real1)sin(radians / 2)), qubit);
}

/// x axis rotation gate - Rotates as e^(-i*\theta/2) around Pauli x axis
void QInterface::RX(real1_f radians, bitLenInt qubit)
{
    real1 cosine = (real1)cos(radians / 2);
    real1 sine = (real1)sin(radians / 2);
    complex pauliRX[4] = { complex(cosine, ZERO_R1), complex(ZERO_R1, -sine), complex(ZERO_R1, -sine),
        complex(cosine, ZERO_R1) };
    ApplySingleBit(pauliRX, qubit);
}

/// y axis rotation gate - Rotates as e^(-i*\theta/2) around Pauli y axis
void QInterface::RY(real1_f radians, bitLenInt qubit)
{
    real1 cosine = (real1)cos(radians / 2);
    real1 sine = (real1)sin(radians / 2);
    complex pauliRY[4] = { cosine, -sine, sine, cosine };
    ApplySingleBit(pauliRY, qubit);
}

/// z axis rotation gate - Rotates as e^(-i*\theta/2) around Pauli z axis
void QInterface::RZ(real1_f radians, bitLenInt qubit)
{
    real1 cosine = (real1)cos(radians / 2);
    real1 sine = (real1)sin(radians / 2);
    ApplySinglePhase(complex(cosine, -sine), complex(cosine, sine), qubit);
}

/// General unitary gate
void QInterface::U(bitLenInt target, real1_f theta, real1_f phi, real1_f lambda)
{
    real1 cos0 = (real1)cos(theta / 2);
    real1 sin0 = (real1)sin(theta / 2);
    const complex uGate[4] = { complex(cos0, ZERO_R1), sin0 * complex((real1)(-cos(lambda)), (real1)(-sin(lambda))),
        sin0 * complex((real1)cos(phi), (real1)sin(phi)),
        cos0 * complex((real1)cos(phi + lambda), (real1)sin(phi + lambda)) };
    ApplySingleBit(uGate, target);
}

/// Apply general unitary gate to each bit in "length," starting from bit index "start"
void QInterface::U(bitLenInt start, bitLenInt length, real1_f theta, real1_f phi, real1_f lambda)
{
    for (bitLenInt bit = 0; bit < length; bit++) {
        U(start + bit, theta, phi, lambda);
    }
}

/// Controlled general unitary gate
void QInterface::CU(
    bitLenInt* controls, bitLenInt controlLen, bitLenInt target, real1_f theta, real1_f phi, real1_f lambda)
{
    real1 cos0 = (real1)cos(theta / 2);
    real1 sin0 = (real1)sin(theta / 2);
    const complex uGate[4] = { complex(cos0, ZERO_R1), sin0 * complex((real1)(-cos(lambda)), (real1)(-sin(lambda))),
        sin0 * complex((real1)cos(phi), (real1)sin(phi)),
        cos0 * complex((real1)cos(phi + lambda), (real1)sin(phi + lambda)) };
    ApplyControlledSingleBit(controls, controlLen, target, uGate);
}

/// Apply 2-parameter unitary gate to each bit in "length," starting from bit index "start"
void QInterface::U2(bitLenInt start, bitLenInt length, real1_f phi, real1_f lambda)
{
    for (bitLenInt bit = 0; bit < length; bit++) {
        U2(start + bit, phi, lambda);
    }
}

/// Uniformly controlled y axis rotation gate - Rotates as e^(-i*\theta_k/2) around Pauli y axis for each permutation
/// "k" of the control bits.
void QInterface::UniformlyControlledRY(
    const bitLenInt* controls, const bitLenInt& controlLen, bitLenInt qubitIndex, const real1* angles)
{
    bitCapIntOcl permCount = pow2Ocl(controlLen);
    std::unique_ptr<complex[]> pauliRYs(new complex[4U * (bitCapIntOcl)permCount]);

    real1 cosine, sine;
    for (bitCapIntOcl i = 0; i < permCount; i++) {
        cosine = cos(angles[i] / 2);
        sine = sin(angles[i] / 2);

        pauliRYs[0U + 4U * i] = complex(cosine, ZERO_R1);
        pauliRYs[1U + 4U * i] = complex(-sine, ZERO_R1);
        pauliRYs[2U + 4U * i] = complex(sine, ZERO_R1);
        pauliRYs[3U + 4U * i] = complex(cosine, ZERO_R1);
    }

    UniformlyControlledSingleBit(controls, controlLen, qubitIndex, pauliRYs.get());
}

/// Uniformly controlled z axis rotation gate - Rotates as e^(-i*\theta_k/2) around Pauli z axis for each permutation
/// "k" of the control bits.
void QInterface::UniformlyControlledRZ(
    const bitLenInt* controls, const bitLenInt& controlLen, bitLenInt qubitIndex, const real1* angles)
{
    bitCapIntOcl permCount = pow2Ocl(controlLen);
    std::unique_ptr<complex[]> pauliRZs(new complex[4U * (bitCapIntOcl)permCount]);

    real1 cosine, sine;
    for (bitCapIntOcl i = 0; i < permCount; i++) {
        cosine = cos(angles[i] / 2);
        sine = sin(angles[i] / 2);

        pauliRZs[0U + 4U * i] = complex(cosine, -sine);
        pauliRZs[1U + 4U * i] = complex(ZERO_R1, ZERO_R1);
        pauliRZs[2U + 4U * i] = complex(ZERO_R1, ZERO_R1);
        pauliRZs[3U + 4U * i] = complex(cosine, sine);
    }

    UniformlyControlledSingleBit(controls, controlLen, qubitIndex, pauliRZs.get());
}

/// Exponentiate identity operator
void QInterface::Exp(real1_f radians, bitLenInt qubit)
{
    complex phaseFac = complex((real1)cos(radians), (real1)sin(radians));
    ApplySinglePhase(phaseFac, phaseFac, qubit);
}

/// Imaginary exponentiate of arbitrary single bit gate
void QInterface::Exp(bitLenInt* controls, bitLenInt controlLen, bitLenInt qubit, complex* matrix2x2, bool antiCtrled)
{
    complex timesI[4];
    complex toApply[4];
    for (bitLenInt i = 0; i < 4; i++) {
        timesI[i] = complex(ZERO_R1, ONE_R1) * matrix2x2[i];
    }
    exp2x2(timesI, toApply);
    if (controlLen == 0) {
        ApplySingleBit(toApply, qubit);
    } else if (antiCtrled) {
        ApplyAntiControlledSingleBit(controls, controlLen, qubit, toApply);
    } else {
        ApplyControlledSingleBit(controls, controlLen, qubit, toApply);
    }
}

/// Exponentiate Pauli X operator
void QInterface::ExpX(real1_f radians, bitLenInt qubit)
{
    complex phaseFac = complex((real1)cos(radians), (real1)sin(radians));
    ApplySingleInvert(phaseFac, phaseFac, qubit);
}

/// Exponentiate Pauli Y operator
void QInterface::ExpY(real1_f radians, bitLenInt qubit)
{
    complex phaseFac = complex((real1)cos(radians), (real1)sin(radians));
    ApplySingleInvert(phaseFac * complex(ZERO_R1, -ONE_R1), phaseFac * complex(ZERO_R1, ONE_R1), qubit);
}

/// Exponentiate Pauli Z operator
void QInterface::ExpZ(real1_f radians, bitLenInt qubit)
{
    complex phaseFac = complex((real1)cos(radians), (real1)sin(radians));
    ApplySinglePhase(phaseFac, -phaseFac, qubit);
}

/// Controlled "phase shift gate" - if control bit is true, rotates target bit as e^(-i*\theta/2) around |1> state
void QInterface::CRT(real1_f radians, bitLenInt control, bitLenInt target)
{
    bitLenInt controls[1] = { control };
    ApplyControlledSinglePhase(
        controls, 1, target, complex(ONE_R1, ZERO_R1), complex((real1)cos(radians / 2), (real1)sin(radians / 2)));
}

/// Controlled x axis rotation - if control bit is true, rotates as e^(-i*\theta/2) around Pauli x axis
void QInterface::CRX(real1_f radians, bitLenInt control, bitLenInt target)
{
    real1 cosine = (real1)cos(radians / 2);
    real1 sine = (real1)sin(radians / 2);
    complex pauliRX[4] = { complex(cosine, ZERO_R1), complex(ZERO_R1, sine), complex(ZERO_R1, sine),
        complex(cosine, ZERO_R1) };
    bitLenInt controls[1] = { control };
    ApplyControlledSingleBit(controls, 1, target, pauliRX);
}

/// Controlled y axis rotation - if control bit is true, rotates as e^(-i*\theta) around Pauli y axis
void QInterface::CRY(real1_f radians, bitLenInt control, bitLenInt target)
{
    real1 cosine = (real1)cos(radians / 2);
    real1 sine = (real1)sin(radians / 2);
    complex pauliRY[4] = { complex(cosine, ZERO_R1), complex(-sine, ZERO_R1), complex(sine, ZERO_R1),
        complex(cosine, ZERO_R1) };
    bitLenInt controls[1] = { control };
    ApplyControlledSingleBit(controls, 1, target, pauliRY);
}

/// Controlled z axis rotation - if control bit is true, rotates as e^(-i*\theta) around Pauli z axis
void QInterface::CRZ(real1_f radians, bitLenInt control, bitLenInt target)
{
    real1 cosine = (real1)cos(radians / 2);
    real1 sine = (real1)sin(radians / 2);
    bitLenInt controls[1] = { control };
    ApplyControlledSinglePhase(controls, 1, target, complex(cosine, -sine), complex(cosine, sine));
}

} // namespace Qrack
