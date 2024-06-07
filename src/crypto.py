from cryptography.hazmat.primitives.asymmetric import rsa
from cryptography.hazmat.primitives.asymmetric import padding
from cryptography.hazmat.primitives import hashes
from cryptography.hazmat.primitives import serialization
from cryptography.hazmat.primitives.asymmetric import utils
from tree import Tree
# from proto.HotStuff_pb2 import VoteRequest



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
        encoding=serialization.Encoding.Raw,
        format=serialization.PrivateFormat.Raw,
        encryption_algorithm=serialization.NoEncryption())
    
    serializedPk = sk.public_key().public_bytes(
        encoding=serialization.Encoding.Raw,
        format=serialization.PublicFormat.Raw)
    
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
    for i in range(2):
        sk, pk = generateRSAkeypair()
        serialized_sk, seralized_pk = serializeRSAKeys(sk)
        print(type(seralized_pk))
        print(f"Client {i} pk : {seralized_pk.decode('utf-8')}")
        print(f"Client {i} sk : {serialized_sk.decode('utf-8')}")
    
    # signature = signMessage(message, sk)
    # result = "Signing and verification was "
    # if verifySignature(message, pk, signature):
    #     result += "successful"
    # else:
    #     result += "unsuccessful"
    # print(result)

#############################
# BLS SIGNATURE using blspy #
#############################
def thresholdKeyGen(numKeys) -> tuple[list[PrivateKey], list[G1Element]]:
    seed: bytes = bytes([ 50, 6,  244, 24,  199, 1,  25,  52,  88,  192,
                        19, 18, 12, 89,  6,   220, 18, 102, 58,  209, 82,
                        12, 62, 89, 110, 182, 9,   44, 20,  254, 22])

    sks : list[G2Element] = []
    for i in range(numKeys):
        sks.append(PopSchemeMPL.key_gen(bytes([i]) + seed))
    return [sks, [sk.get_g1() for sk in sks]]

def clientthresholdKeyGen(numKeys) -> tuple[list[PrivateKey], list[G1Element]]:
    seed: bytes = bytes([ 50, 6,  244, 24,  199, 1,  25,  52,  88,  192,
                        19, 18, 12, 89,  6,   220, 18, 102, 58,  209, 82,
                        12, 62, 89, 110, 182, 9,   44, 20,  254, 22])
    
    sks : list[G2Element] = []
    for i in range(numKeys):
        sks.append(PopSchemeMPL.key_gen(seed + bytes([i])))
    return [sks, [sk.get_g1() for sk in sks]]

def aggregatePks(pks : list[G1Element]) -> G1Element:
    assert len(pks) > 0
    agg_pk : G1Element = pks[0]
    for i in range(1, len(pks)):
        agg_pk += pks[i]
    return agg_pk

def partialSign(sk : PrivateKey, message : bytes) -> G2Element:
    return PopSchemeMPL.sign(sk, message)

def verifySigs(message : bytes, sigs : list[bytes], pks : list[G1Element]) -> tuple[bool, G2Element]:
    decodedSigs = [G2Element.from_bytes(sig) for sig in sigs]
    aggSig = PopSchemeMPL.aggregate(decodedSigs)
    return PopSchemeMPL.fast_aggregate_verify(pks, message, aggSig), aggSig


def testBLSThreshold(numKeys):
    sks, pks = thresholdKeyGen(numKeys)
    aggPk = aggregatePks(pks)
    message : bytes = b"This is a test message"
    sigs : list[G2Element] = [partialSign(sk, message) for sk in sks]

    for i in range(numKeys):
        aggSig = PopSchemeMPL.aggregate(sigs[:i+1])
        aggPk = pks[:i+1]
        ok = PopSchemeMPL.fast_aggregate_verify(aggPk, message, aggSig)
        print(f"Verification of {i}: {ok}")
    
    print(verifyThreshold(4, sigs[:3], pks[:3], message))

    


def verifyThreshold(t: int, sigs : list[G2Element], pks: list[G1Element], message):
    assert len(sigs) == len(pks)
    if len(sigs) < t:
        print(f"Error: Fewer than {t} signatures")
        return False

    aggSig = PopSchemeMPL.aggregate(sigs)
    return PopSchemeMPL.fast_aggregate_verify(pks, message, aggSig)

def parsePK(pk_str : str) -> G1Element:
    pk_bytes = bytes.fromhex(pk_str)
    return G1Element.from_bytes(pk_bytes)

def parseSK(sk_str : str) -> G2Element:
    sk_bytes = bytes.fromhex(sk_str)
    return PrivateKey.from_bytes(sk_bytes)

def seralizePKSK(pk: G1Element, sk : PrivateKey) -> tuple[str, str]:
    return bytes(pk).hex(), bytes(sk).hex()


def getRootQCSiganture():
    myTree = Tree("r0", "deef")
    rootNode = myTree.get_root_node()
    data = VoteRequest.Data(viewNumber=0, node=rootNode.to_bytes())
    msg = data.SerializeToString()
    sks, pks = thresholdKeyGen(4)
    sigs = []
    for sk in sks:
        sigs.append(partialSign(sk, msg))
    
    print(f"The partial sign worked {verifyThreshold(4, sigs, pks, msg)}")
    print(PopSchemeMPL.aggregate(sigs))

    
    



# # testRSA()
# sks, pks = clientthresholdKeyGen(5)
# for i in range(5):
#     serliazed_pk, serliazed_sk = seralizePKSK(pks[i], sks[i])
#     print(f"Client {i} sk: {serliazed_sk}")
#     print(f"Client {i} pk: {serliazed_pk}")


# # testBLSThreshold(4)
# sks, pks = thresholdKeyGen(5)
# for i in range(len(sks)):
#     pk_hex, sk_hex = seralizePKSK(pks[i], sks[i])
#     print(f"Hex PK {i}: {pk_hex}\nSK {i}: {sk_hex}\n")

# # getRootQCSiganture()



