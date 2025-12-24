package email

import (
	"context"
	"fmt"
	"time"

	"github.com/aws/aws-sdk-go-v2/aws"
	"github.com/aws/aws-sdk-go-v2/config"
	"github.com/aws/aws-sdk-go-v2/service/ses"
	"github.com/aws/aws-sdk-go-v2/service/ses/types"
)

// EmailService handles sending emails via AWS SES
type EmailService struct {
	client    *ses.Client
	fromEmail string
	fromName  string
	baseURL   string
}

// NewEmailService creates a new email service using AWS SES
func NewEmailService(region, fromEmail, fromName, baseURL string) (*EmailService, error) {
	ctx, cancel := context.WithTimeout(context.Background(), 30*time.Second)
	defer cancel()

	cfg, err := config.LoadDefaultConfig(ctx,
		config.WithRegion(region),
	)
	if err != nil {
		return nil, fmt.Errorf("failed to load AWS config: %w", err)
	}

	client := ses.NewFromConfig(cfg)

	return &EmailService{
		client:    client,
		fromEmail: fromEmail,
		fromName:  fromName,
		baseURL:   baseURL,
	}, nil
}

// SendPasswordResetEmail sends a password reset email with the reset token
func (e *EmailService) SendPasswordResetEmail(ctx context.Context, toEmail, resetToken string) error {
	// Build reset URL - this should point to the web app's password reset page
	// The web app will extract the token and call POST /api/v1/auth/reset-password/confirm
	resetURL := fmt.Sprintf("%s/reset-password?token=%s", e.baseURL, resetToken)

	subject := "Reset Your Sidechain Password"
	htmlBody := fmt.Sprintf(`
		<!DOCTYPE html>
		<html>
		<head>
			<meta charset="UTF-8">
			<style>
				body { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif; line-height: 1.6; color: #333; }
				.container { max-width: 600px; margin: 0 auto; padding: 20px; }
				.button { display: inline-block; padding: 12px 24px; background-color: #00d4ff; color: white; text-decoration: none; border-radius: 6px; margin: 20px 0; }
				.button:hover { background-color: #00b8e0; }
			</style>
		</head>
		<body>
			<div class="container">
				<h1>Reset Your Password</h1>
				<p>You requested to reset your password for your Sidechain account.</p>
				<p>Click the button below to reset your password. This link will expire in 1 hour.</p>
				<a href="%s" class="button">Reset Password</a>
				<p>Or copy and paste this link into your browser:</p>
				<p style="word-break: break-all; color: #666;">%s</p>
				<p>If you didn't request this password reset, you can safely ignore this email.</p>
				<hr>
				<p style="color: #999; font-size: 12px;">This is an automated message from Sidechain.</p>
			</div>
		</body>
		</html>
	`, resetURL, resetURL)

	textBody := fmt.Sprintf(`
Reset Your Sidechain Password

You requested to reset your password for your Sidechain account.

Click the link below to reset your password. This link will expire in 1 hour.

%s

If you didn't request this password reset, you can safely ignore this email.

This is an automated message from Sidechain.
	`, resetURL)

	// Build the email
	from := e.fromEmail
	if e.fromName != "" {
		from = fmt.Sprintf("%s <%s>", e.fromName, e.fromEmail)
	}

	input := &ses.SendEmailInput{
		Source: aws.String(from),
		Destination: &types.Destination{
			ToAddresses: []string{toEmail},
		},
		Message: &types.Message{
			Subject: &types.Content{
				Data:    aws.String(subject),
				Charset: aws.String("UTF-8"),
			},
			Body: &types.Body{
				Html: &types.Content{
					Data:    aws.String(htmlBody),
					Charset: aws.String("UTF-8"),
				},
				Text: &types.Content{
					Data:    aws.String(textBody),
					Charset: aws.String("UTF-8"),
				},
			},
		},
	}

	// Send the email
	_, err := e.client.SendEmail(ctx, input)
	if err != nil {
		return fmt.Errorf("failed to send password reset email: %w", err)
	}

	return nil
}
