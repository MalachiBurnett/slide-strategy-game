import { TestSuite, assert } from "./testUtils";
import { validateUsername, validatePassword } from "../validation";

export async function runValidationTests() {
  const suite = new TestSuite();

  await suite.test("Username Validation: Valid", () => {
    assert(validateUsername("player1").valid, "Standard username should be valid");
    assert(validateUsername("Player_One").valid, "Underscores and caps should be valid");
  });

  await suite.test("Username Validation: Invalid", () => {
    assert(!validateUsername("").valid, "Empty username should be invalid");
    assert(!validateUsername("too_long_of_a_username_more_than_20").valid, "Too long username should be invalid");
    assert(!validateUsername("user!").valid, "Special characters should be invalid");
  });

  await suite.test("Password Validation: Valid", () => {
    assert(validatePassword("SecureP@ss1").valid, "Strong password should be valid");
  });

  await suite.test("Password Validation: Weak", () => {
    assert(!validatePassword("weak").valid, "Short password should be invalid");
    assert(!validatePassword("NoSpecial1").valid, "No special char should be invalid");
    assert(!validatePassword("alllowercase!1").valid, "No uppercase should be invalid");
    assert(!validatePassword("NoNumber!").valid, "No number should be invalid");
  });

  suite.report();
}
