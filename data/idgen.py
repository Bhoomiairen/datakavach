"""Generators for syntactically valid Indian identifiers (synthetic test data only).

Every function here produces values that pass the same checks real IDs pass (Verhoeff for
Aadhaar, mod-36 for GSTIN, Luhn for cards), so the detector can be tested without real data.
None of these numbers belongs to a real person on purpose, but some may collide with real ones
by chance: never use them outside tests.
"""
import random
import string

D = [[0,1,2,3,4,5,6,7,8,9],[1,2,3,4,0,6,7,8,9,5],[2,3,4,0,1,7,8,9,5,6],[3,4,0,1,2,8,9,5,6,7],
     [4,0,1,2,3,9,5,6,7,8],[5,9,8,7,6,0,4,3,2,1],[6,5,9,8,7,1,0,4,3,2],[7,6,5,9,8,2,1,0,4,3],
     [8,7,6,5,9,3,2,1,0,4],[9,8,7,6,5,4,3,2,1,0]]
P = [[0,1,2,3,4,5,6,7,8,9],[1,5,7,6,2,8,3,0,9,4],[5,8,0,3,7,9,6,1,4,2],[8,9,1,6,0,4,3,5,2,7],
     [9,4,5,3,1,2,6,8,7,0],[4,2,8,6,5,7,3,9,0,1],[2,7,9,3,8,0,6,4,1,5],[7,0,4,6,9,1,3,2,5,8]]
INV = [0,4,3,2,1,5,6,7,8,9]
B36 = string.digits + string.ascii_uppercase


def verhoeff_check_digit(num: str) -> str:
    c = 0
    for i, ch in enumerate(reversed(num)):
        c = D[c][P[(i + 1) % 8][int(ch)]]
    return str(INV[c])


def verhoeff_valid(num: str) -> bool:
    c = 0
    for i, ch in enumerate(reversed(num)):
        c = D[c][P[i % 8][int(ch)]]
    return c == 0


def aadhaar(rng: random.Random) -> str:
    while True:
        body = str(rng.randint(2, 9)) + "".join(rng.choice(string.digits) for _ in range(10))
        num = body + verhoeff_check_digit(body)
        if len(set(num)) > 1:
            return num


def aadhaar_invalid(rng: random.Random) -> str:
    """12 digits, first digit 2-9, but the check digit is wrong."""
    while True:
        num = str(rng.randint(2, 9)) + "".join(rng.choice(string.digits) for _ in range(11))
        if not verhoeff_valid(num):
            return num


def pan(rng: random.Random, holder: str = "P") -> str:
    letters = string.ascii_uppercase
    return ("".join(rng.choice(letters) for _ in range(3)) + holder + rng.choice(letters)
            + "".join(rng.choice(string.digits) for _ in range(4)) + rng.choice(letters))


def gstin_check_char(first14: str) -> str:
    total = 0
    for i, ch in enumerate(first14):
        prod = B36.index(ch) * (2 if i % 2 else 1)
        total += prod // 36 + prod % 36
    return B36[(36 - total % 36) % 36]


def gstin(rng: random.Random) -> str:
    state = f"{rng.randint(1, 37):02d}"
    body = state + pan(rng, rng.choice("CFP")) + rng.choice("123456789") + "Z"
    return body + gstin_check_char(body)


def luhn_check_digit(num: str) -> str:
    total = 0
    for i, ch in enumerate(reversed(num)):
        d = int(ch)
        if i % 2 == 0:
            d *= 2
            if d > 9:
                d -= 9
        total += d
    return str((10 - total % 10) % 10)


def card(rng: random.Random) -> str:
    prefix = rng.choice(["4", "51", "52", "53", "54", "55", "60", "65", "508"])
    body = prefix + "".join(rng.choice(string.digits) for _ in range(15 - len(prefix)))
    return body + luhn_check_digit(body)


def mobile(rng: random.Random) -> str:
    return str(rng.randint(6, 9)) + "".join(rng.choice(string.digits) for _ in range(9))


def ifsc(rng: random.Random) -> str:
    bank = rng.choice(["SBIN", "HDFC", "ICIC", "UTIB", "PUNB", "KKBK", "BARB", "CNRB", "IDIB", "YESB"])
    return bank + "0" + "".join(rng.choice(string.digits) for _ in range(6))


def upi(rng: random.Random, name: str) -> str:
    handle = rng.choice(["okaxis", "oksbi", "okhdfcbank", "okicici", "ybl", "paytm", "ibl", "axl", "apl"])
    local = rng.choice([name.lower().replace(" ", "."), name.split()[0].lower() + str(rng.randint(1, 999)),
                        mobile(rng)])
    return f"{local}@{handle}"


def voter_id(rng: random.Random) -> str:
    return "".join(rng.choice(string.ascii_uppercase) for _ in range(3)) + "".join(
        rng.choice(string.digits) for _ in range(7))


def passport(rng: random.Random) -> str:
    first = rng.choice("ABCDEFGHJKLMNPRSTUVWY")
    return first + str(rng.randint(1, 9)) + "".join(rng.choice(string.digits) for _ in range(5)) + str(
        rng.randint(1, 9))


def bank_account(rng: random.Random) -> str:
    n = rng.choice([11, 12, 14, 16])
    return str(rng.randint(1, 9)) + "".join(rng.choice(string.digits) for _ in range(n - 1))


if __name__ == "__main__":
    r = random.Random(7)
    print("aadhaar", aadhaar(r), aadhaar(r))
    print("aadhaar_invalid", aadhaar_invalid(r))
    print("pan", pan(r), pan(r, "C"))
    print("gstin", gstin(r), gstin(r))
    print("card", card(r), card(r))
    print("ifsc", ifsc(r))
    print("voter", voter_id(r), "passport", passport(r))
