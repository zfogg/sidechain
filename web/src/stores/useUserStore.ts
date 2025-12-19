import { create } from 'zustand';
import { persist } from 'zustand/middleware';
import type { User } from '../models/User';
import { apiClient } from '../api/client';

/**
 * User store - Handles authentication and current user state
 * Mirrors C++ UserStore pattern
 */

interface UserStoreState {
  user: User | null;
  token: string | null;
  isAuthenticated: boolean;
  isLoading: boolean;
  error: string;
}

interface UserStoreActions {
  login: (token: string, user: User) => void;
  logout: () => void;
  setUser: (user: User) => void;
  updateUser: (updates: Partial<User>) => void;
  setLoading: (loading: boolean) => void;
  setError: (error: string) => void;
  clearError: () => void;
  restoreSession: () => Promise<void>;
}

export const useUserStore = create<UserStoreState & UserStoreActions>()(
  persist(
    (set, get) => ({
      user: null,
      token: null,
      isAuthenticated: false,
      isLoading: false,
      error: '',

      login: (token, user) => {
        apiClient.setToken(token);
        set({
          user,
          token,
          isAuthenticated: true,
          error: '',
        });
      },

      logout: () => {
        apiClient.clearToken();
        set({
          user: null,
          token: null,
          isAuthenticated: false,
          error: '',
        });
      },

      setUser: (user) => {
        set({ user });
      },

      updateUser: (updates) => {
        const current = get().user;
        if (current) {
          set({
            user: { ...current, ...updates },
          });
        }
      },

      setLoading: (loading) => {
        set({ isLoading: loading });
      },

      setError: (error) => {
        set({ error });
      },

      clearError: () => {
        set({ error: '' });
      },

      restoreSession: async () => {
        const storedToken = localStorage.getItem('auth_token');
        const storedUserStore = localStorage.getItem('user-store');

        if (storedToken) {
          try {
            apiClient.setToken(storedToken);

            // Restore user from localStorage cache immediately
            let cachedUser = null;
            if (storedUserStore) {
              try {
                const parsed = JSON.parse(storedUserStore);
                cachedUser = parsed.state?.user;
              } catch (e) {
                console.error('Failed to parse cached user store:', e);
              }
            }

            // Set state immediately with cached user so header renders
            set({
              token: storedToken,
              isAuthenticated: true,
              user: cachedUser,
              isLoading: true,
            });

            // Fetch fresh user data - this is critical for showing actual user info
            try {
              const response = await apiClient.get<User>('/users/me');
              if (response.isOk()) {
                let freshUser = response.getValue();

                // If /users/me doesn't return profile picture, fetch from /users/{username}/profile
                // to ensure we have complete profile data for the header
                if (freshUser && !freshUser.profilePictureUrl && freshUser.username) {
                  try {
                    const profileResponse = await apiClient.get<any>(`/users/${freshUser.username}/profile`);
                    if (profileResponse.isOk()) {
                      const profileData = profileResponse.getValue();
                      freshUser = {
                        ...freshUser,
                        profilePictureUrl: profileData.profilePictureUrl || profileData.profile_picture_url || ''
                      };
                    }
                  } catch (e) {
                    // Silently fail - use what we have
                  }
                }

                set({ user: freshUser, isLoading: false });
              } else {
                console.error('Failed to fetch user data:', response);
                set({ isLoading: false });
              }
            } catch (error) {
              console.error('Error fetching user data during session restore:', error);
              set({ isLoading: false });
              // If fetch fails, keep using cached user - don't logout
            }
          } catch (error) {
            console.error('Session restore error:', error);
            apiClient.clearToken();
            localStorage.removeItem('auth_token');
            set({
              token: null,
              isAuthenticated: false,
              isLoading: false,
              error: 'Session expired',
              user: null,
            });
          }
        }
      },
    }),
    {
      name: 'user-store',
      partialize: (state) => ({
        user: state.user,
        token: state.token,
        isAuthenticated: state.isAuthenticated,
      }),
    }
  )
);
