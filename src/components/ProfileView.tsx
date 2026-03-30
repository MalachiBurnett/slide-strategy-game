import React, { useState } from 'react';
import { motion, AnimatePresence } from 'motion/react';
import { ArrowLeft, User, Mail, Lock, CheckCircle, AlertCircle, Loader2, AlertTriangle } from 'lucide-react';
import { UserData } from '../types/game';
import { validateUsername } from '../utils/validation';

interface ProfileViewProps {
  user: UserData;
  onBack: () => void;
  onUpdateUser: (updatedUser: Partial<UserData>) => void;
}

export const ProfileView: React.FC<ProfileViewProps> = ({ user, onBack, onUpdateUser }) => {
  const [newUsername, setNewUsername] = useState(user.username);
  const [newEmail, setNewEmail] = useState('');
  const [complaint, setComplaint] = useState('');
  const [status, setStatus] = useState<'idle' | 'loading' | 'success' | 'error'>('idle');
  const [message, setMessage] = useState('');
  const [usernameError, setUsernameError] = useState<string>('');

  const handleReport = async () => {
    if (!complaint) return;
    setStatus('loading');
    try {
      const res = await fetch('/api/report', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ complaint })
      });
      const data = await res.json();
      if (res.ok) {
        setStatus('success');
        setMessage('Report sent successfully!');
        setComplaint('');
      } else {
        setStatus('error');
        setMessage(data.error);
      }
    } catch (e) {
      setStatus('error');
      setMessage('Network error');
    }
  };

  const handleUpdateUsername = async () => {
    if (newUsername === user.username) return;
    if (user.is_guest) {
      setStatus('error');
      setMessage('Guests cannot change username');
      return;
    }
    
    // Validate username
    const validation = validateUsername(newUsername);
    if (!validation.valid) {
      setStatus('error');
      setMessage(validation.error || 'Invalid username');
      return;
    }
    
    setStatus('loading');
    try {
      const res = await fetch('/api/profile/username', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ username: newUsername })
      });
      const data = await res.json();
      if (res.ok) {
        onUpdateUser({ username: newUsername });
        setStatus('success');
        setMessage('Username updated successfully!');
        setUsernameError('');
      } else {
        setStatus('error');
        setMessage(data.error);
      }
    } catch (e) {
      setStatus('error');
      setMessage('Network error');
    }
  };

  const handlePasswordReset = async () => {
    if (user.is_guest) {
      setStatus('error');
      setMessage('Guests cannot reset password');
      return;
    }
    setStatus('loading');
    try {
      const res = await fetch('/api/profile/password-reset-request', { method: 'POST' });
      const data = await res.json();
      if (res.ok) {
        setStatus('success');
        setMessage('Password reset link sent to your email!');
      } else {
        setStatus('error');
        setMessage(data.error);
      }
    } catch (e) {
      setStatus('error');
      setMessage('Network error');
    }
  };

  const handleEmailChange = async () => {
    if (!newEmail) return;
    setStatus('loading');
    try {
      const res = await fetch('/api/profile/email-change-request', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ newEmail })
      });
      const data = await res.json();
      if (res.ok) {
        setStatus('success');
        setMessage('Verification email sent to your new address!');
      } else {
        setStatus('error');
        setMessage(data.error);
      }
    } catch (e) {
      setStatus('error');
      setMessage('Network error');
    }
  };

  return (
    <div className="min-h-screen bg-[var(--bg)] text-[var(--text)] p-4 sm:p-8 font-sans transition-colors duration-500">
      <div className="max-w-2xl mx-auto">
        <header className="flex items-center gap-6 mb-12">
          <button 
            onClick={onBack}
            className="p-3 bg-[var(--primary)] rounded-full shadow-lg hover:scale-110 transition-transform"
          >
            <ArrowLeft className="w-6 h-6 text-[var(--primaryText)]" />
          </button>
          <h1 className="text-4xl font-black">Profile Settings</h1>
        </header>

        <div className="space-y-8">
          {/* Username Section */}
          <section className="bg-[var(--bgLight)] p-8 rounded-3xl shadow-xl border-b-4 border-[var(--primary)] border-opacity-10">
            <h2 className="text-xl font-bold mb-6 flex items-center gap-2">
              <User className="w-5 h-5 text-[var(--primary)]" />
              Change Username
            </h2>
            {user.is_guest ? (
              <div className="p-4 bg-red-500 bg-opacity-10 rounded-2xl flex items-center gap-3 text-white font-bold border border-red-500 border-opacity-20">
                <AlertTriangle className="w-5 h-5" />
                <p>Guest accounts cannot change username. Create an account to customize your name.</p>
              </div>
            ) : (
              <>
                <div className="flex flex-col sm:flex-row gap-4">
                  <div className="flex-1">
                    <input 
                      type="text" 
                      value={newUsername}
                      onChange={(e) => {
                        setNewUsername(e.target.value);
                        if (e.target.value && e.target.value !== user.username) {
                          const validation = validateUsername(e.target.value);
                          setUsernameError(validation.valid ? '' : validation.error || '');
                        } else {
                          setUsernameError('');
                        }
                      }}
                      placeholder="New Username"
                      className={`w-full px-6 py-4 bg-[var(--bg)] rounded-2xl outline-none focus:ring-2 border font-bold ${
                        usernameError ? 'border-red-500 focus:ring-red-500' : 'border-[var(--primary)] border-opacity-10 focus:ring-[var(--primary)]'
                      }`}
                    />
                    {usernameError && (
                      <p className="mt-2 text-xs text-red-500 font-medium flex items-center gap-1">
                        <AlertCircle className="w-3 h-3" />
                        {usernameError}
                      </p>
                    )}
                    {newUsername && newUsername !== user.username && !usernameError && (
                      <p className="mt-2 text-xs text-green-500 font-medium flex items-center gap-1">
                        <CheckCircle className="w-3 h-3" />
                        Username looks good
                      </p>
                    )}
                  </div>
                  <button 
                    onClick={handleUpdateUsername}
                    disabled={status === 'loading' || newUsername === user.username || !!usernameError}
                    className="px-8 py-4 bg-[var(--primary)] text-[var(--primaryText)] rounded-2xl font-bold hover:opacity-90 transition-all disabled:opacity-50 whitespace-nowrap"
                  >
                    Update
                  </button>
                </div>
                <p className="text-xs opacity-40 mt-3">Changing your name will also appeal any active public match bans.</p>
              </>
            )}
          </section>

          {/* Password Section */}
          <section className="bg-[var(--bgLight)] p-8 rounded-3xl shadow-xl border-b-4 border-[var(--primary)] border-opacity-10">
            <h2 className="text-xl font-bold mb-6 flex items-center gap-2">
              <Lock className="w-5 h-5 text-[var(--primary)]" />
              Account Security
            </h2>
            {user.is_guest ? (
              <div className="p-4 bg-red-500 bg-opacity-10 rounded-2xl flex items-center gap-3 text-white font-bold border border-red-500 border-opacity-20">
                <AlertTriangle className="w-5 h-5" />
                <p>Guest accounts cannot reset password. Create an account to secure your profile.</p>
              </div>
            ) : (
              <>
                <p className="opacity-60 mb-6">Need to change your password? We'll send a secure reset link to your registered email address.</p>
                <button 
                  onClick={handlePasswordReset}
                  disabled={status === 'loading'}
                  className="w-full py-4 border-2 border-[var(--primary)] text-[var(--primary)] rounded-2xl font-bold hover:bg-[var(--primary)] hover:text-[var(--primaryText)] transition-all flex items-center justify-center gap-2"
                >
                  <Mail className="w-5 h-5" />
                  Send Password Reset Email
                </button>
              </>
            )}
          </section>

          {/* Email Section */}
          <section className="bg-[var(--bgLight)] p-8 rounded-3xl shadow-xl border-b-4 border-[var(--primary)] border-opacity-10">
            <h2 className="text-xl font-bold mb-6 flex items-center gap-2">
              <Mail className="w-5 h-5 text-[var(--primary)]" />
              Change Email
            </h2>
            {user.is_guest ? (
              <div className="p-4 bg-red-500 bg-opacity-10 rounded-2xl flex items-center gap-3 text-white font-bold border border-red-500 border-opacity-20">
                <AlertTriangle className="w-5 h-5" />
                <p>Guest accounts do not have email. Create an account to add email.</p>
              </div>
            ) : (
              <>
                <p className="opacity-60 mb-6">You'll need to verify your new email address before the change takes effect.</p>
                <div className="flex flex-col sm:flex-row gap-4">
                  <input 
                    type="email" 
                    value={newEmail}
                    onChange={(e) => setNewEmail(e.target.value)}
                    placeholder="New Email Address"
                    className="flex-1 px-6 py-4 bg-[var(--bg)] rounded-2xl outline-none focus:ring-2 focus:ring-[var(--primary)] border border-[var(--primary)] border-opacity-10 font-bold"
                  />
                  <button 
                    onClick={handleEmailChange}
                    disabled={status === 'loading' || !newEmail}
                    className="px-8 py-4 bg-[var(--primary)] text-[var(--primaryText)] rounded-2xl font-bold hover:opacity-90 transition-all disabled:opacity-50"
                  >
                    Change
                  </button>
                </div>
              </>
            )}
          </section>

          {/* Report Section */}
          <section className="bg-[var(--bgLight)] p-8 rounded-3xl shadow-xl border-b-4 border-red-500 border-opacity-10">
            <h2 className="text-xl font-bold mb-6 flex items-center gap-2 text-red-500">
              <AlertTriangle className="w-5 h-5" />
              Report a Problem
            </h2>
            <p className="opacity-60 mb-6">Found a bug or have a complaint? Let us know below. This will be sent directly to the developers.</p>
            <div className="flex flex-col gap-4">
              <textarea 
                value={complaint}
                onChange={(e) => setComplaint(e.target.value)}
                placeholder="Describe your issue or complaint here..."
                className="w-full px-6 py-4 bg-[var(--bg)] rounded-2xl outline-none focus:ring-2 focus:ring-red-500 border border-red-500 border-opacity-10 font-medium min-h-[120px] resize-none"
              />
              <button 
                onClick={handleReport}
                disabled={status === 'loading' || !complaint}
                className="w-full py-4 bg-red-500 text-white rounded-2xl font-bold hover:opacity-90 transition-all disabled:opacity-50 flex items-center justify-center gap-2 shadow-lg shadow-red-500/20"
              >
                <AlertTriangle className="w-5 h-5" />
                Submit Report
              </button>
            </div>
          </section>
        </div>

        {/* Status Message */}
        <AnimatePresence>
          {status !== 'idle' && (
            <motion.div 
              initial={{ opacity: 0, y: 20 }}
              animate={{ opacity: 1, y: 0 }}
              exit={{ opacity: 0, y: 20 }}
              className={`mt-8 p-6 rounded-2xl text-center font-bold flex items-center justify-center gap-3 ${
                status === 'loading' ? 'bg-[var(--primary)] bg-opacity-5 text-[var(--primary)]' :
                status === 'success' ? 'bg-green-500 bg-opacity-10 text-green-600' :
                'bg-red-500 bg-opacity-10 text-red-600'
              }`}
            >
              {status === 'loading' && <Loader2 className="animate-spin" />}
              {status === 'success' && <CheckCircle />}
              {status === 'error' && <AlertCircle />}
              {status === 'loading' ? 'Processing...' : message}
            </motion.div>
          )}
        </AnimatePresence>
      </div>
    </div>
  );
};

// Reset Password View (separate component for the reset page)
export const ResetPasswordView: React.FC<{ token: string, onDone: () => void }> = ({ token, onDone }) => {
  const [password, setPassword] = useState('');
  const [status, setStatus] = useState<'idle' | 'loading' | 'success' | 'error'>('idle');
  const [error, setError] = useState('');

  const handleReset = async (e: React.FormEvent) => {
    e.preventDefault();
    setStatus('loading');
    try {
      const res = await fetch('/api/profile/password-reset-confirm', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ token, newPassword: password })
      });
      if (res.ok) setStatus('success');
      else {
        const data = await res.json();
        setError(data.error);
        setStatus('error');
      }
    } catch (e) {
      setError('Connection error');
      setStatus('error');
    }
  };

  return (
    <div className="min-h-screen bg-[var(--bg)] flex items-center justify-center p-4 text-[var(--text)]">
      <motion.div 
        initial={{ opacity: 0, scale: 0.9 }}
        animate={{ opacity: 1, scale: 1 }}
        className="bg-[var(--bgLight)] p-10 rounded-[2.5rem] shadow-2xl w-full max-w-md border-b-8 border-[var(--primary)]"
      >
        <h1 className="text-3xl font-black mb-6 text-center">Set New Password</h1>
        
        {status === 'success' ? (
          <div className="text-center">
            <CheckCircle className="w-16 h-16 text-green-500 mx-auto mb-4" />
            <p className="text-xl font-bold mb-8">Password changed!</p>
            <button onClick={onDone} className="w-full py-4 bg-[var(--primary)] text-[var(--primaryText)] rounded-2xl font-bold">
              Back to Login
            </button>
          </div>
        ) : (
          <form onSubmit={handleReset} className="space-y-6">
            <div className="space-y-2">
              <label className="text-sm font-bold opacity-60">New Password</label>
              <input 
                type="password" 
                required
                value={password}
                onChange={(e) => setPassword(e.target.value)}
                className="w-full px-6 py-4 bg-[var(--bg)] rounded-2xl border border-[var(--primary)] border-opacity-10 outline-none focus:ring-2 focus:ring-[var(--primary)] font-bold"
              />
            </div>
            {status === 'error' && <p className="text-red-500 font-bold text-center">{error}</p>}
            <button 
              type="submit"
              disabled={status === 'loading'}
              className="w-full py-5 bg-[var(--primary)] text-[var(--primaryText)] rounded-2xl font-black text-xl flex items-center justify-center gap-2"
            >
              {status === 'loading' && <Loader2 className="animate-spin" />}
              Reset Password
            </button>
          </form>
        )}
      </motion.div>
    </div>
  );
};
