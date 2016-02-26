import collections
import nacl.hash
import nacl.utils
from nacl.public import Box
from nacl.signing import SigningKey, VerifyKey
from nacl.encoding import RawEncoder

class NetjoinAuthsession(object):

    def __init__(self):

        # Our keys, the signing and private keys never leave this module
        self._my_signing_key = SigningKey.generate()
        self.my_verify_key = self._my_signing_key.verify_key

        self._my_private_key = self._my_signing_key.to_curve25519_private_key()
        self.my_public_key = self._my_private_key.public_key

        # Their keys - populated by set_their_verify_key()
        self.their_verify_key = None
        self.their_public_key = None

        # Challenges sent by announcer
        self.recent_challenges = collections.deque(maxlen=20)

    def is_recent_challenge(self, challenge):
        return challenge in self.recent_challenges

    # Record all recent challenges from outgoing beacons
    def sent_challenge(self, challenge):
        # Newest first because it is most likely to see a response
        self.recent_challenges.appendleft(challenge)

    def sha512(self, data):
        return nacl.hash.sha512(data, RawEncoder)

    def get_verify_key(self):
        return self.my_verify_key

    def get_raw_verify_key(self):
        return self.my_verify_key.encode(encoder=RawEncoder)

    def set_their_raw_verify_key(self, raw_verify_key):
        their_verify_key = VerifyKey(raw_verify_key, encoder=RawEncoder)
        self.set_their_verify_key(their_verify_key)

    # Save the other party's public verification key
    def set_their_verify_key(self, their_verify_key):
        self.their_verify_key = their_verify_key
        self.their_public_key = their_verify_key.to_curve25519_public_key()
        self.box = Box(self._my_private_key, self.their_public_key)

    # Generate a random nonce for an encryption box
    def get_nonce(self):
        return nacl.utils.random(Box.NONCE_SIZE)

    # Encrypt given data with nonce, returns encrypted data and nonce combined
    def encrypt(self, data, nonce):
        assert self.their_verify_key is not None
        return self.box.encrypt(data, nonce)

    # Decrypt a message from the other party
    def decrypt(self, encrypted):
        assert self.their_verify_key is not None
        return self.box.decrypt(encrypted)

def main():
    auth_alice = NetjoinAuthsession()
    auth_bob = NetjoinAuthsession()

    # Exchange public keys between alice and bob
    auth_alice.set_their_verify_key(auth_bob.get_verify_key())
    auth_bob.set_their_verify_key(auth_alice.get_verify_key())

    message = b"Secret sauce"

    alice_secret_message = auth_alice.encrypt(message, auth_alice.get_nonce())

    alice_message = auth_bob.decrypt(alice_secret_message)
    if alice_message == message:
        print "PASSED: NetjoinAuthsession test"

if __name__ == "__main__":
    main()
