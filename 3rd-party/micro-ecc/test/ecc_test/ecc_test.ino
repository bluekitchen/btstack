#include <uECC.h>

extern "C" {

static int RNG(uint8_t *dest, unsigned size) {
  // Use the least-significant bits from the ADC for an unconnected pin (or connected to a source of 
  // random noise). This can take a long time to generate random data if the result of analogRead(0) 
  // doesn't change very frequently.
  while (size) {
    uint8_t val = 0;
    for (unsigned i = 0; i < 8; ++i) {
      int init = analogRead(0);
      int count = 0;
      while (analogRead(0) == init) {
        ++count;
      }
      
      if (count == 0) {
         val = (val << 1) | (init & 0x01);
      } else {
         val = (val << 1) | (count & 0x01);
      }
    }
    *dest = val;
    ++dest;
    --size;
  }
  // NOTE: it would be a good idea to hash the resulting random data using SHA-256 or similar.
  return 1;
}

}  // extern "C"

void setup() {
  Serial.begin(115200);
  Serial.print("Testing ecc\n");
  uECC_set_rng(&RNG);
}

void loop() {
  uint8_t private1[uECC_BYTES];
  uint8_t private2[uECC_BYTES];
  
  uint8_t public1[uECC_BYTES * 2];
  uint8_t public2[uECC_BYTES * 2];
  
  uint8_t secret1[uECC_BYTES];
  uint8_t secret2[uECC_BYTES];
  
  unsigned long a = millis();
  uECC_make_key(public1, private1);
  unsigned long b = millis();
  
  Serial.print("Made key 1 in "); Serial.println(b-a);
  a = millis();
  uECC_make_key(public2, private2);
  b = millis();
  Serial.print("Made key 2 in "); Serial.println(b-a);

  a = millis();
  int r = uECC_shared_secret(public2, private1, secret1);
  b = millis();
  Serial.print("Shared secret 1 in "); Serial.println(b-a);
  if (!r) {
    Serial.print("shared_secret() failed (1)\n");
    return;
  }

  a = millis();
  r = uECC_shared_secret(public1, private2, secret2);
  b = millis();
  Serial.print("Shared secret 2 in "); Serial.println(b-a);
  if (!r) {
    Serial.print("shared_secret() failed (2)\n");
    return;
  }
    
  if (memcmp(secret1, secret2, sizeof(secret1)) != 0) {
    Serial.print("Shared secrets are not identical!\n");
  } else {
    Serial.print("Shared secrets are identical\n");
  }
}

