from cryptography.hazmat.primitives.asymmetric import rsa
from cryptography.hazmat.primitives.asymmetric import padding
from cryptography.hazmat.primitives import hashes
from cryptography.hazmat.primitives import serialization
from cryptography.hazmat.primitives.asymmetric import utils

from blspy import (PrivateKey, Util, AugSchemeMPL, PopSchemeMPL,
                   G1Element, G2Element)
import hashlib

def generateRSAkeypair():
    # Generate RSA private and public key pair
    sk = rsa.generate_private_key(
        public_exponent=65537,
        key_size=2048,
    )
    # use privateKey.public_key()
    return sk, sk.public_key()

def serializeRSAKeys(sk):
    serializedSk =  sk.private_bytes(
        encoding=serialization.Encoding.PEM,
        format=serialization.PrivateFormat.PKCS8,
        encryption_algorithm=serialization.NoEncryption())
    
    serializedPk = sk.public_key().public_bytes(
        encoding=serialization.Encoding.PEM,
        format=serialization.PublicFormat.SubjectPublicKeyInfo)
    
    return serializedSk, serializedPk

def signMessage(msg, sk):
    return sk.sign(
        msg,
        padding.PSS(
            mgf=padding.MGF1(hashes.SHA256()),
            salt_length=padding.PSS.MAX_LENGTH
        ),
        hashes.SHA256())

def verifySignature(msg, pk, sig):
    try:
        pk.verify(
            sig,
            msg,
            padding.PSS(
                mgf=padding.MGF1(hashes.SHA256()),
                salt_length=padding.PSS.MAX_LENGTH
            ),
            hashes.SHA256()
        )
        return True
    except:
        return False

def testRSA():
    message = b"A message I want to sign"
    sk, pk = generateRSAkeypair()
    signature = signMessage(message, sk)
    result = "Signing and verification was "
    if verifySignature(message, pk, signature):
        result += "successful"
    else:
        result += "unsuccessful"
    print(result)

#############################
# BLS SIGNATURE using blspy #
#############################
def thresholdKeyGen(numKeys) -> list[list[PrivateKey], list[G1Element]]:
    seed: bytes = bytes([ 50, 6,  244, 24,  199, 1,  25,  52,  88,  192,
                        19, 18, 12, 89,  6,   220, 18, 102, 58,  209, 82,
                        12, 62, 89, 110, 182, 9,   44, 20,  254, 22])

    sks : list[G2Element] = []
    for i in range(numKeys):
        sks.append(PopSchemeMPL.key_gen(bytes([i]) + seed))
    return [sks, [sk.get_g1() for sk in sks]]

def aggregatePks(pks : list[G1Element]) -> G1Element:
    assert len(pks) > 0
    agg_pk : G1Element = pks[0]
    for i in range(1, len(pks)):
        agg_pk += pks[i]
    return agg_pk

def partialSign(sk : PrivateKey, message : bytes) -> G2Element:
    return PopSchemeMPL.sign(sk, message)

def testBLSThreshold(numKeys):
    sks, pks = thresholdKeyGen(numKeys)
    aggPk = aggregatePks(pks)
    message : bytes = b"This is a test message"
    sigs = [partialSign(sk, message) for sk in sks]

    for i in range(numKeys):
        aggSig = PopSchemeMPL.aggregate(sigs[i+2:i+4])
        aggPk = pks
        ok = PopSchemeMPL.fast_aggregate_verify(aggPk, message, aggSig)
        print(f"Verification of {i}: {ok}")


################################
# SCHORR Threshold using blspy #
################################




testRSA()
testBLSThreshold(4)